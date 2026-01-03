#include "pch.h"
#include "include/WinFileSystem.h"

#include <filesystem>
#include <string>
#include <vector>
#include "include/Platform.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace
{
    using Drama::Core::IO::FsError;
    using Drama::Core::IO::FsResult;

    static FsError map_win_error_to_fs(DWORD e) noexcept
    {
        switch (e)
        {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return FsError::NotFound;
        case ERROR_ACCESS_DENIED:  return FsError::AccessDenied;
        default:                  return FsError::IoError;
        }
    }

    static FsResult make_error(FsError e, DWORD lastErr = ::GetLastError(), const char* msg = nullptr) noexcept
    {
        FsResult r;
        r.error = e;
        r.ec = std::error_code(static_cast<int>(lastErr), std::system_category());
        if (msg)
        {
            r.message = msg;
        }
        return r;
    }

    /// @brief Windows API向けに区切り文字を統一する
    /// @param w 
    static void normalize_slashes(std::wstring& w) noexcept
    {
        for (auto& ch : w)
        {
            if (ch == L'/') ch = L'\\';
        }
    }

    struct UniqueHandle
    {
        HANDLE m_handle = INVALID_HANDLE_VALUE;
        UniqueHandle() = default;
        explicit UniqueHandle(HANDLE handle) : m_handle(handle) {}
        ~UniqueHandle() { if (m_handle != INVALID_HANDLE_VALUE) { ::CloseHandle(m_handle); } }
        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;
        UniqueHandle(UniqueHandle&& o) noexcept : m_handle(o.m_handle) { o.m_handle = INVALID_HANDLE_VALUE; }
        UniqueHandle& operator=(UniqueHandle&& o) noexcept
        {
            if (this != &o)
            {
                if (m_handle != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(m_handle);
                }
                m_handle = o.m_handle;
                o.m_handle = INVALID_HANDLE_VALUE;
            }
            return *this;
        }
        explicit operator bool() const noexcept { return m_handle != INVALID_HANDLE_VALUE; }
    };

    static bool write_all(HANDLE handle, const void* data, size_t size) noexcept
    {
        constexpr DWORD k_maxChunk = 0x7FFFFFFF;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        size_t remaining = size;

        while (remaining > 0)
        {
            const DWORD chunk = (remaining > k_maxChunk) ? k_maxChunk : static_cast<DWORD>(remaining);
            DWORD written = 0;
            if (!::WriteFile(handle, p, chunk, &written, nullptr))
            {
                return false;
            }
            if (written != chunk)
            {
                return false;
            }

            p += chunk;
            remaining -= chunk;
        }
        return true;
    }
}

namespace Drama::Platform::IO
{
    FsResult WinFileSystem::exists(std::string_view path) noexcept
    {
        // 1) 不正なパスでI/Oを試みないために検証する
        if (path.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Path is empty.");
        }
        // 2) Windows APIに渡すためUTF-16へ変換する
        std::filesystem::path p = std::filesystem::path(Windows::to_utf16(std::string(path)));
        // 3) 失敗理由を分岐できるように存在確認する
        std::error_code ec;
        bool exists = std::filesystem::exists(p, ec);
        if (exists)
        {
            return FsResult::Ok();
        }
        else
        {
            if (ec)
            {
                return make_error(FsError::IoError, 0, "Failed to check existence due to IO error.");
            }
            return make_error(FsError::NotFound, 0, "Path not found.");
        }
    }
    FsResult WinFileSystem::create_directories(std::string_view path) noexcept
    {
        // 1) 不正なパスで作成を試みないために検証する
        if (path.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Path is empty.");
        }
        // 2) Windows APIに渡すためUTF-16へ変換する
        std::filesystem::path p = std::filesystem::path(Windows::to_utf16(std::string(path)));
        // 3) 既存時の挙動を判断できるように作成する
        std::error_code ec;
        bool created = std::filesystem::create_directories(p, ec);
        if (created)
        {
            return FsResult::Ok();
        }
        else
        {
            if (ec)
            {
                return make_error(FsError::IoError, 0, "Failed to create directories due to IO error.");
            }
            else
            {
                // 既に存在していた場合など
                return FsResult::Ok();
            }
        }
    }

    /// @brief 上書き
    /// @param path 
    /// @param data 
    /// @param size 
    /// @return 
    Drama::Core::IO::FsResult WinFileSystem::write_all_bytes(std::string_view path, const void* data, size_t size) noexcept
    {
        // 1) 無効入力で誤書き込みを避ける
        if (path.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return make_error(FsError::IoError, 0, "Data is null.");
        }

        // 2) Windows APIに渡すためUTF-16へ変換する
        std::wstring w = Windows::to_utf16(std::string(path));
        normalize_slashes(w);
        if (w.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Utf8ToWide failed.");
        }

        // 必要なら親ディレクトリ作成をここでやる（方針次第）
        // create_directories(parent_dir_utf8(path)) とかを呼ぶ設計でも良い

        UniqueHandle h(::CreateFileW(
            w.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,            // 読み取りは許可（ログやホットリロードで便利）
            nullptr,
            CREATE_ALWAYS,              // 上書き
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (!h)
        {
            return make_error(FsError::IoError, ::GetLastError(), "CreateFileW(write) failed.");
        }

        if (!write_all(h.m_handle, data, size))
        {
            return make_error(FsError::IoError, ::GetLastError(), "WriteFile failed.");
        }

        return FsResult::Ok();
    }

    FsResult WinFileSystem::read_all_bytes(std::string_view path, std::vector<uint8_t>& out) noexcept
    {
        // 1) 失敗時に残骸が残らないよう初期化し、不正入力を弾く
        out.clear();

        if (path.empty())
            return make_error(FsError::InvalidPath, 0, "Path is empty.");

        // 2) Windows APIに渡すためUTF-16へ変換する
        std::wstring w = Windows::to_utf16(std::string(path));
        normalize_slashes(w);
        if (w.empty())
            return make_error(FsError::InvalidPath, 0, "Utf8ToWide failed.");

        // 読み取り：他が書いてても読めるように share を広めにする（必要に応じて調整）
        UniqueHandle h(::CreateFileW(
            w.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr));

        if (!h)
        {
            const DWORD e = ::GetLastError();
            return make_error(map_win_error_to_fs(e), e, "CreateFileW(read) failed.");
        }

        LARGE_INTEGER sz{};
        if (!::GetFileSizeEx(h.m_handle, &sz))
        {
            const DWORD e = ::GetLastError();
            return make_error(FsError::IoError, e, "GetFileSizeEx failed.");
        }
        if (sz.QuadPart < 0)
            return make_error(FsError::IoError, 0, "Invalid file size.");

        // vectorサイズに入らないレベルはこのAPIの責務外。必要ならストリーム読みを別途作れ。
        const uint64_t fileSize = (uint64_t)sz.QuadPart;
        if (fileSize > (uint64_t)std::vector<uint8_t>().max_size())
            return make_error(FsError::IoError, 0, "File too large for vector.");

        out.resize((size_t)fileSize);

        // ReadFileはDWORD単位。大きいファイルに備えてループで読む。
        constexpr DWORD k_maxChunk = 0x7FFFFFFF;
        uint8_t* dst = out.data();
        size_t remaining = out.size();

        while (remaining > 0)
        {
            const DWORD chunk = (remaining > k_maxChunk) ? k_maxChunk : static_cast<DWORD>(remaining);

            DWORD readBytes = 0;
            if (!::ReadFile(h.m_handle, dst, chunk, &readBytes, nullptr))
            {
                const DWORD e = ::GetLastError();
                out.clear();
                return make_error(FsError::IoError, e, "ReadFile failed.");
            }

            // EOF等で想定より短い読みが起きた場合（通常は起きないが保険）
            if (readBytes == 0)
            {
                out.resize(out.size() - remaining);
                break;
            }

            dst += readBytes;
            remaining -= readBytes;
        }

        return FsResult::Ok();
    }

    /// @brief 追記
    /// @param path 
    /// @param data 
    /// @param size 
    /// @return 
    Drama::Core::IO::FsResult WinFileSystem::append_all_bytes(std::string_view path, const void* data, size_t size) noexcept
    {
        // 1) 無効入力で誤書き込みを避ける
        if (path.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return make_error(FsError::IoError, 0, "Data is null.");
        }

        // 2) Windows APIに渡すためUTF-16へ変換する
        std::wstring w = Windows::to_utf16(std::string(path));
        normalize_slashes(w);
        if (w.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Utf8ToWide failed.");
        }

        UniqueHandle h(::CreateFileW(
            w.c_str(),
            FILE_APPEND_DATA,                   // 追記
            FILE_SHARE_READ | FILE_SHARE_WRITE, // ログは読む側がいてもよい
            nullptr,
            OPEN_ALWAYS,                        // 無ければ作る
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (!h)
        {
            return make_error(FsError::IoError, ::GetLastError(), "CreateFileW(append) failed.");
        }

        if (!write_all(h.m_handle, data, size))
        {
            return make_error(FsError::IoError, ::GetLastError(), "WriteFile(append) failed.");
        }

        return FsResult::Ok();
    }

    /// @brief 安全な保存
    /// @param path 
    /// @param data 
    /// @param size 
    /// @return 
    Drama::Core::IO::FsResult WinFileSystem::write_all_bytes_atomic(std::string_view path, const void* data, size_t size) noexcept
    {
        // 1) 無効入力で誤書き込みを避ける
        if (path.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return make_error(FsError::IoError, 0, "Data is null.");
        }

        // 2) 既存ファイルを壊さないため一時ファイル名を作る
        // 一時ファイル名（雑でOKだが衝突は避ける）
        std::string tmpPath = std::string(path) + ".tmp";

        // 3) Windows APIに渡すためUTF-16へ変換する
        std::wstring wTmp = Windows::to_utf16(tmpPath);
        std::wstring wDst = Windows::to_utf16(std::string(path));
        normalize_slashes(wTmp);
        normalize_slashes(wDst);

        if (wTmp.empty() || wDst.empty())
        {
            return make_error(FsError::InvalidPath, 0, "Utf8ToWide failed.");
        }

        // 4) 成功時のみ本番に置換するためtmpに書き込む
        // tmp に書く
        {
            UniqueHandle h(::CreateFileW(
                wTmp.c_str(),
                GENERIC_WRITE,
                0,                      // tmpは排他でいい
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (!h)
            {
                return make_error(FsError::IoError, ::GetLastError(), "CreateFileW(tmp) failed.");
            }

            if (!write_all(h.m_handle, data, size))
            {
                return make_error(FsError::IoError, ::GetLastError(), "WriteFile(tmp) failed.");
            }

            // 重要：ディスクに吐かせたいならフラッシュ（設定/セーブなら推奨）
            if (!::FlushFileBuffers(h.m_handle))
            {
                return make_error(FsError::IoError, ::GetLastError(), "FlushFileBuffers failed.");
            }
        }

        // 5) 途中失敗で壊れないよう原子的に置換する
        // tmp を本番に置換（原子的）
        if (!::MoveFileExW(
            wTmp.c_str(),
            wDst.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            const DWORD e = ::GetLastError();
            ::DeleteFileW(wTmp.c_str()); // 失敗時は掃除
            return make_error(FsError::IoError, e, "MoveFileExW(replace) failed.");
        }

        return FsResult::Ok();
    }
    std::string WinFileSystem::current_path() noexcept
    {
        return std::filesystem::current_path().string();
    }
}
