#include "pch.h"
#include "include/WinFileSystem.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <vector>
#include <string>
#include <filesystem>
#include <functional>
#include "include/Platform.h"

namespace
{
    using Drama::Core::IO::FsError;
    using Drama::Core::IO::FsResult;

    static FsError MapWinErrorToFs(DWORD e) noexcept
    {
        switch (e)
        {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return FsError::NotFound;
        case ERROR_ACCESS_DENIED:  return FsError::AccessDenied;
        default:                  return FsError::IoError;
        }
    }

    static FsResult MakeError(FsError e, DWORD lastErr = ::GetLastError(), const char* msg = nullptr) noexcept
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

    /// @brief パス文字列中の区切り文字を統一する補助関数
    /// @param w 
    static void NormalizeSlashes(std::wstring& w) noexcept
    {
        for (auto& ch : w)
        {
            if (ch == L'/') ch = L'\\';
        }
    }

    struct UniqueHandle
    {
        HANDLE h = INVALID_HANDLE_VALUE;
        UniqueHandle() = default;
        explicit UniqueHandle(HANDLE handle) : h(handle) {}
        ~UniqueHandle() { if (h != INVALID_HANDLE_VALUE) { ::CloseHandle(h); } }
        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;
        UniqueHandle(UniqueHandle&& o) noexcept : h(o.h) { o.h = INVALID_HANDLE_VALUE; }
        UniqueHandle& operator=(UniqueHandle&& o) noexcept
        {
            if (this != &o)
            {
                if (h != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(h);
                }
                h = o.h;
                o.h = INVALID_HANDLE_VALUE;
            }
            return *this;
        }
        explicit operator bool() const noexcept { return h != INVALID_HANDLE_VALUE; }
    };

    static bool WriteAll(HANDLE h, const void* data, size_t size) noexcept
    {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        size_t remaining = size;

        while (remaining > 0)
        {
            const DWORD chunk = (remaining > 0x7FFFFFFF) ? 0x7FFFFFFF : static_cast<DWORD>(remaining);
            DWORD written = 0;
            if (!::WriteFile(h, p, chunk, &written, nullptr))
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
    FsResult WinFileSystem::Exists(std::string_view path) noexcept
    {
        if(path.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Path is empty.");
        }
        std::filesystem::path p = std::filesystem::path(Windows::ToUTF16(std::string(path)));
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
                return MakeError(FsError::IoError, 0, "Failed to check existence due to IO error.");
            }
            return MakeError(FsError::NotFound, 0, "Path not found.");
        }
    }
    FsResult WinFileSystem::CreateDirectories(std::string_view path) noexcept
    {
        if (path.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Path is empty.");
        }
        std::filesystem::path p = std::filesystem::path(Windows::ToUTF16(std::string(path)));
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
                return MakeError(FsError::IoError, 0, "Failed to create directories due to IO error.");
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
    Drama::Core::IO::FsResult WinFileSystem::WriteAllBytes(std::string_view path, const void* data, size_t size) noexcept
    {
        if (path.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return MakeError(FsError::IoError, 0, "Data is null.");
        }

        std::wstring w = Windows::ToUTF16(std::string(path));
        NormalizeSlashes(w);
        if (w.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Utf8ToWide failed.");
        }

        // 必要なら親ディレクトリ作成をここでやる（方針次第）
        // CreateDirectories(ParentDirUtf8(path)) とかを呼ぶ設計でも良い

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
            return MakeError(FsError::IoError, ::GetLastError(), "CreateFileW(write) failed.");
        }

        if (!WriteAll(h.h, data, size))
        {
            return MakeError(FsError::IoError, ::GetLastError(), "WriteFile failed.");
        }

        return FsResult::Ok();
    }

    FsResult WinFileSystem::ReadAllBytes(std::string_view path, std::vector<uint8_t>& out) noexcept
    {
        out.clear();

        if (path.empty())
            return MakeError(FsError::InvalidPath, 0, "Path is empty.");

        std::wstring w = Windows::ToUTF16(std::string(path));
        NormalizeSlashes(w);
        if (w.empty())
            return MakeError(FsError::InvalidPath, 0, "Utf8ToWide failed.");

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
            return MakeError(MapWinErrorToFs(e), e, "CreateFileW(read) failed.");
        }

        LARGE_INTEGER sz{};
        if (!::GetFileSizeEx(h.h, &sz))
        {
            const DWORD e = ::GetLastError();
            return MakeError(FsError::IoError, e, "GetFileSizeEx failed.");
        }
        if (sz.QuadPart < 0)
            return MakeError(FsError::IoError, 0, "Invalid file size.");

        // vectorサイズに入らないレベルはこのAPIの責務外。必要ならストリーム読みを別途作れ。
        const uint64_t fileSize = (uint64_t)sz.QuadPart;
        if (fileSize > (uint64_t)std::vector<uint8_t>().max_size())
            return MakeError(FsError::IoError, 0, "File too large for vector.");

        out.resize((size_t)fileSize);

        // ReadFileはDWORD単位。大きいファイルに備えてループで読む。
        uint8_t* dst = out.data();
        size_t remaining = out.size();

        while (remaining > 0)
        {
            const DWORD chunk = (remaining > 0x7FFFFFFF) ? 0x7FFFFFFF : (DWORD)remaining;

            DWORD readBytes = 0;
            if (!::ReadFile(h.h, dst, chunk, &readBytes, nullptr))
            {
                const DWORD e = ::GetLastError();
                out.clear();
                return MakeError(FsError::IoError, e, "ReadFile failed.");
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
    Drama::Core::IO::FsResult WinFileSystem::AppendAllBytes(std::string_view path, const void* data, size_t size) noexcept
    {
        if (path.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return MakeError(FsError::IoError, 0, "Data is null.");
        }

        std::wstring w = Windows::ToUTF16(std::string(path));
        NormalizeSlashes(w);
        if (w.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Utf8ToWide failed.");
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
            return MakeError(FsError::IoError, ::GetLastError(), "CreateFileW(append) failed.");
        }

        if (!WriteAll(h.h, data, size))
        {
            return MakeError(FsError::IoError, ::GetLastError(), "WriteFile(append) failed.");
        }

        return FsResult::Ok();
    }

    /// @brief 安全な保存
    /// @param path 
    /// @param data 
    /// @param size 
    /// @return 
    Drama::Core::IO::FsResult WinFileSystem::WriteAllBytesAtomic(std::string_view path, const void* data, size_t size) noexcept
    {
        if (path.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return MakeError(FsError::IoError, 0, "Data is null.");
        }

        // 一時ファイル名（雑でOKだが衝突は避ける）
        std::string tmpPath = std::string(path) + ".tmp";

        std::wstring wTmp = Windows::ToUTF16(tmpPath);
        std::wstring wDst = Windows::ToUTF16(std::string(path));
        NormalizeSlashes(wTmp);
        NormalizeSlashes(wDst);

        if (wTmp.empty() || wDst.empty())
        {
            return MakeError(FsError::InvalidPath, 0, "Utf8ToWide failed.");
        }

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
                return MakeError(FsError::IoError, ::GetLastError(), "CreateFileW(tmp) failed.");
            }

            if (!WriteAll(h.h, data, size))
            {
                return MakeError(FsError::IoError, ::GetLastError(), "WriteFile(tmp) failed.");
            }

            // 重要：ディスクに吐かせたいならフラッシュ（設定/セーブなら推奨）
            if (!::FlushFileBuffers(h.h))
            {
                return MakeError(FsError::IoError, ::GetLastError(), "FlushFileBuffers failed.");
            }
        }

        // tmp を本番に置換（原子的）
        if (!::MoveFileExW(
            wTmp.c_str(),
            wDst.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            const DWORD e = ::GetLastError();
            ::DeleteFileW(wTmp.c_str()); // 失敗時は掃除
            return MakeError(FsError::IoError, e, "MoveFileExW(replace) failed.");
        }

        return FsResult::Ok();
    }
    std::string WinFileSystem::currentPath() noexcept
    {
        return std::filesystem::current_path().string();
    }
}
