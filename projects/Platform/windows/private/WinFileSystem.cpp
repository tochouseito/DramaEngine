// WinFileSystem.cpp

// 1) 自分のヘッダ
#include "windows/private/WinFileSystem.h"

// 2) C++ Standard Library
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <source_location>

// 3) Windows
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace
{
    // =========================
    // Result 生成（あなたのResult実装に合わせてここだけ調整すればよい）
    // =========================
    static Drama::Core::Error::Result make_ok() noexcept
    {
        // 1) 成功を返す
        return Drama::Core::Error::Result::ok();
    }

    static Drama::Core::Error::Code map_win32_to_code(DWORD e) noexcept
    {
        // 1) よくある Win32 エラーをエンジン共通コードへ
        switch (e)
        {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        {
            return Drama::Core::Error::Code::NotFound;
        }
        case ERROR_ACCESS_DENIED:
        {
            return Drama::Core::Error::Code::AccessDenied;
        }
        case ERROR_INVALID_NAME:
        case ERROR_BAD_PATHNAME:
        {
            return Drama::Core::Error::Code::InvalidArg;
        }
        default:
        {
            return Drama::Core::Error::Code::IoError;
        }
        }
    }

    static Drama::Core::Error::Result make_fail_io(DWORD e, std::string_view msg,
        const std::source_location& loc = std::source_location::current()) noexcept
    {
        // 1) IO系の失敗を返す（facilityはIOに寄せる）
        return Drama::Core::Error::Result::fail(
            Drama::Core::Error::Facility::IO,
            map_win32_to_code(e),
            Drama::Core::Error::Severity::Error,
            static_cast<uint32_t>(e),
            msg,
            loc);
    }

    static Drama::Core::Error::Result make_fail_invalid(std::string_view msg,
        const std::source_location& loc = std::source_location::current()) noexcept
    {
        // 1) 引数不正
        return Drama::Core::Error::Result::fail(
            Drama::Core::Error::Facility::IO,
            Drama::Core::Error::Code::InvalidArg,
            Drama::Core::Error::Severity::Error,
            0,
            msg,
            loc);
    }

    // =========================
    // RAII: HANDLE
    // =========================
    struct UniqueHandle final
    {
        HANDLE m_h = INVALID_HANDLE_VALUE;

        UniqueHandle() noexcept = default;

        explicit UniqueHandle(HANDLE h) noexcept
            : m_h(h)
        {
        }

        ~UniqueHandle() noexcept
        {
            // 1) 有効なら閉じる
            if (m_h != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_h);
                m_h = INVALID_HANDLE_VALUE;
            }
        }

        UniqueHandle(const UniqueHandle&) = delete;
        UniqueHandle& operator=(const UniqueHandle&) = delete;

        UniqueHandle(UniqueHandle&& rhs) noexcept
            : m_h(rhs.m_h)
        {
            // 1) ムーブ元を無効化
            rhs.m_h = INVALID_HANDLE_VALUE;
        }

        UniqueHandle& operator=(UniqueHandle&& rhs) noexcept
        {
            // 1) 自己代入防止
            if (this != &rhs)
            {
                // 2) 既存を閉じる
                if (m_h != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(m_h);
                }

                // 3) 受け取る
                m_h = rhs.m_h;
                rhs.m_h = INVALID_HANDLE_VALUE;
            }

            return *this;
        }

        bool is_valid() const noexcept
        {
            // 1) 有効判定
            return m_h != INVALID_HANDLE_VALUE;
        }
    };

    // =========================
    // UTF-8 <-> UTF-16
    // =========================
    static std::wstring utf8_to_wide(std::string_view s) noexcept
    {
        // 1) 空対策
        if (s.empty())
        {
            return {};
        }

        // 2) 必要長取得
        const int len = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        if (len <= 0)
        {
            return {};
        }

        // 3) 変換
        std::wstring w;
        w.resize(static_cast<size_t>(len));
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);

        return w;
    }

    static std::string wide_to_utf8(std::wstring_view w) noexcept
    {
        // 1) 空対策
        if (w.empty())
        {
            return {};
        }

        // 2) 必要長取得
        const int len = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0)
        {
            return {};
        }

        // 3) 変換
        std::string s;
        s.resize(static_cast<size_t>(len));
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), len, nullptr, nullptr);

        return s;
    }

    static void normalize_slashes(std::wstring& w) noexcept
    {
        // 1) Windows用に'/'を'\'へ寄せる
        for (auto& ch : w)
        {
            if (ch == L'/')
            {
                ch = L'\\';
            }
        }
    }

    static std::string parent_dir_utf8(std::string_view path) noexcept
    {
        // 1) 末尾の区切りを無視したいならここで削る（今回は簡易）
        const size_t pos = path.find_last_of("/\\");
        if (pos == std::string_view::npos)
        {
            return {};
        }

        // 2) 親ディレクトリ部分を返す
        return std::string(path.substr(0, pos));
    }

    static bool write_all_loop(HANDLE h, const void* data, size_t size) noexcept
    {
        // 1) サイズ0なら成功
        if (size == 0)
        {
            return true;
        }

        // 2) nullチェック
        if (data == nullptr)
        {
            return false;
        }

        // 3) DWORD制限に合わせて分割書き込み
        const uint8_t* p = static_cast<const uint8_t*>(data);
        size_t remaining = size;

        while (remaining > 0)
        {
            const DWORD chunk = (remaining > 0x7FFFFFFF) ? 0x7FFFFFFF : static_cast<DWORD>(remaining);

            DWORD written = 0;
            if (::WriteFile(h, p, chunk, &written, nullptr) == FALSE)
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

    static bool read_all_loop(HANDLE h, void* dst, size_t size) noexcept
    {
        // 1) サイズ0なら成功
        if (size == 0)
        {
            return true;
        }

        // 2) nullチェック
        if (dst == nullptr)
        {
            return false;
        }

        // 3) DWORD制限に合わせて分割読み込み
        uint8_t* p = static_cast<uint8_t*>(dst);
        size_t remaining = size;

        while (remaining > 0)
        {
            const DWORD chunk = (remaining > 0x7FFFFFFF) ? 0x7FFFFFFF : static_cast<DWORD>(remaining);

            DWORD readBytes = 0;
            if (::ReadFile(h, p, chunk, &readBytes, nullptr) == FALSE)
            {
                return false;
            }

            if (readBytes == 0)
            {
                // EOF
                return (remaining == 0);
            }

            p += readBytes;
            remaining -= readBytes;
        }

        return true;
    }

    static Drama::Core::Error::Result create_directories_w(std::wstring_view wPath,
        const std::source_location& loc = std::source_location::current()) noexcept
    {
        // 1) 空チェック
        if (wPath.empty())
        {
            return make_fail_invalid("Path is empty.", loc);
        }

        // 2) 編集用にコピーして正規化
        std::wstring path(wPath);
        normalize_slashes(path);

        // 3) 末尾'\'を削る（"C:\dir\" -> "C:\dir"）
        while (!path.empty() && (path.back() == L'\\'))
        {
            path.pop_back();
        }

        if (path.empty())
        {
            return make_fail_invalid("Path is empty after trim.", loc);
        }

        // 4) 走査開始位置を決める（ドライブ/UNCの基点を飛ばす）
        size_t start = 0;

        // UNC: \\server\share\...
        if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\')
        {
            // 4-1) \\ の後の server と share を飛ばす
            size_t p = 2;
            size_t slashCount = 0;

            while (p < path.size())
            {
                if (path[p] == L'\\')
                {
                    ++slashCount;
                    if (slashCount >= 2)
                    {
                        ++p;
                        break;
                    }
                }
                ++p;
            }

            start = p;
        }
        // Drive: C:\...
        else if (path.size() >= 2 && path[1] == L':')
        {
            // 4-2) "C:" の直後から
            start = 2;
            if (path.size() >= 3 && path[2] == L'\\')
            {
                start = 3;
            }
        }

        // 5) パスを区切って順に CreateDirectoryW
        for (size_t i = start; i <= path.size(); ++i)
        {
            if (i == path.size() || path[i] == L'\\')
            {
                const std::wstring partial = path.substr(0, i);

                if (partial.empty())
                {
                    continue;
                }

                if (::CreateDirectoryW(partial.c_str(), nullptr) == FALSE)
                {
                    const DWORD e = ::GetLastError();
                    if (e != ERROR_ALREADY_EXISTS)
                    {
                        return make_fail_io(e, "CreateDirectoryW failed.", loc);
                    }
                }
            }
        }

        return make_ok();
    }
}

namespace Drama::Platform::IO
{
    Drama::Core::Error::Result WinFileSystem::exists(std::string_view path) noexcept
    {
        // 1) 引数チェック
        if (path.empty())
        {
            return make_fail_invalid("Path is empty.");
        }

        // 2) UTF-16へ
        std::wstring w = utf8_to_wide(path);
        normalize_slashes(w);
        if (w.empty())
        {
            return make_fail_invalid("Utf8ToWide failed.");
        }

        // 3) 属性取得で存在確認
        const DWORD attr = ::GetFileAttributesW(w.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES)
        {
            const DWORD e = ::GetLastError();
            return make_fail_io(e, "GetFileAttributesW failed.");
        }

        return make_ok();
    }

    Drama::Core::Error::Result WinFileSystem::create_directories(std::string_view path) noexcept
    {
        // 1) 引数チェック
        if (path.empty())
        {
            return make_fail_invalid("Path is empty.");
        }

        // 2) UTF-16へ
        std::wstring w = utf8_to_wide(path);
        normalize_slashes(w);
        if (w.empty())
        {
            return make_fail_invalid("Utf8ToWide failed.");
        }

        // 3) 生成（多段）
        return create_directories_w(w);
    }

    Drama::Core::Error::Result WinFileSystem::write_all_bytes(std::string_view path, const void* data, size_t size) noexcept
    {
        // 1) 引数チェック
        if (path.empty())
        {
            return make_fail_invalid("Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return make_fail_invalid("Data is null.");
        }

        // 2) 親ディレクトリを作る（方針：Write系は親を保証）
        {
            const std::string parent = parent_dir_utf8(path);
            if (!parent.empty())
            {
                const auto r = create_directories(parent);
                if (!r)
                {
                    return r;
                }
            }
        }

        // 3) UTF-16へ
        std::wstring w = utf8_to_wide(path);
        normalize_slashes(w);
        if (w.empty())
        {
            return make_fail_invalid("Utf8ToWide failed.");
        }

        // 4) ファイルを作成/上書きして書き込み
        UniqueHandle h(::CreateFileW(
            w.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (!h.is_valid())
        {
            return make_fail_io(::GetLastError(), "CreateFileW(write) failed.");
        }

        if (!write_all_loop(h.m_h, data, size))
        {
            return make_fail_io(::GetLastError(), "WriteFile failed.");
        }

        return make_ok();
    }

    Drama::Core::Error::Result WinFileSystem::read_all_bytes(std::string_view path, std::vector<uint8_t>& out) noexcept
    {
        // 1) 初期化
        out.clear();

        // 2) 引数チェック
        if (path.empty())
        {
            return make_fail_invalid("Path is empty.");
        }

        // 3) UTF-16へ
        std::wstring w = utf8_to_wide(path);
        normalize_slashes(w);
        if (w.empty())
        {
            return make_fail_invalid("Utf8ToWide failed.");
        }

        // 4) OPEN_EXISTINGで開く
        UniqueHandle h(::CreateFileW(
            w.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
            nullptr));

        if (!h.is_valid())
        {
            return make_fail_io(::GetLastError(), "CreateFileW(read) failed.");
        }

        // 5) サイズ取得
        LARGE_INTEGER sz{};
        if (::GetFileSizeEx(h.m_h, &sz) == FALSE)
        {
            return make_fail_io(::GetLastError(), "GetFileSizeEx failed.");
        }
        if (sz.QuadPart < 0)
        {
            return make_fail_io(0, "Invalid file size.");
        }

        const uint64_t fileSize = static_cast<uint64_t>(sz.QuadPart);
        if (fileSize > static_cast<uint64_t>(out.max_size()))
        {
            return make_fail_io(0, "File too large.");
        }

        // 6) 読み込み
        out.resize(static_cast<size_t>(fileSize));
        if (!read_all_loop(h.m_h, out.data(), out.size()))
        {
            out.clear();
            return make_fail_io(::GetLastError(), "ReadFile failed.");
        }

        return make_ok();
    }

    Drama::Core::Error::Result WinFileSystem::append_all_bytes(std::string_view path, const void* data, size_t size) noexcept
    {
        // 1) 引数チェック
        if (path.empty())
        {
            return make_fail_invalid("Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return make_fail_invalid("Data is null.");
        }

        // 2) 親ディレクトリを作る
        {
            const std::string parent = parent_dir_utf8(path);
            if (!parent.empty())
            {
                const auto r = create_directories(parent);
                if (!r)
                {
                    return r;
                }
            }
        }

        // 3) UTF-16へ
        std::wstring w = utf8_to_wide(path);
        normalize_slashes(w);
        if (w.empty())
        {
            return make_fail_invalid("Utf8ToWide failed.");
        }

        // 4) OPEN_ALWAYS + FILE_APPEND_DATA
        UniqueHandle h(::CreateFileW(
            w.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr));

        if (!h.is_valid())
        {
            return make_fail_io(::GetLastError(), "CreateFileW(append) failed.");
        }

        // 5) 追記
        if (!write_all_loop(h.m_h, data, size))
        {
            return make_fail_io(::GetLastError(), "WriteFile(append) failed.");
        }

        return make_ok();
    }

    Drama::Core::Error::Result WinFileSystem::write_all_bytes_atomic(std::string_view path, const void* data, size_t size) noexcept
    {
        // 1) 引数チェック
        if (path.empty())
        {
            return make_fail_invalid("Path is empty.");
        }
        if (size > 0 && data == nullptr)
        {
            return make_fail_invalid("Data is null.");
        }

        // 2) 親ディレクトリを作る
        {
            const std::string parent = parent_dir_utf8(path);
            if (!parent.empty())
            {
                const auto r = create_directories(parent);
                if (!r)
                {
                    return r;
                }
            }
        }

        // 3) 一時ファイル名を作る（同一ディレクトリ内）
        std::string tmpPath;
        tmpPath.reserve(path.size() + 64);
        tmpPath.append(path.data(), path.size());
        tmpPath += ".tmp.";
        tmpPath += std::to_string(static_cast<uint32_t>(::GetCurrentProcessId()));
        tmpPath += ".";
        tmpPath += std::to_string(static_cast<uint32_t>(::GetCurrentThreadId()));

        // 4) tmpへ書く（Flushありで安全寄り）
        {
            std::wstring wTmp = utf8_to_wide(tmpPath);
            normalize_slashes(wTmp);
            if (wTmp.empty())
            {
                return make_fail_invalid("Utf8ToWide failed for tmp.");
            }

            UniqueHandle h(::CreateFileW(
                wTmp.c_str(),
                GENERIC_WRITE,
                0,
                nullptr,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                nullptr));

            if (!h.is_valid())
            {
                return make_fail_io(::GetLastError(), "CreateFileW(tmp) failed.");
            }

            if (!write_all_loop(h.m_h, data, size))
            {
                return make_fail_io(::GetLastError(), "WriteFile(tmp) failed.");
            }

            if (::FlushFileBuffers(h.m_h) == FALSE)
            {
                return make_fail_io(::GetLastError(), "FlushFileBuffers(tmp) failed.");
            }
        }

        // 5) tmp -> dst を置換（原子的）
        {
            std::wstring wTmp = utf8_to_wide(tmpPath);
            std::wstring wDst = utf8_to_wide(path);
            normalize_slashes(wTmp);
            normalize_slashes(wDst);

            if (wTmp.empty() || wDst.empty())
            {
                return make_fail_invalid("Utf8ToWide failed for move.");
            }

            if (::MoveFileExW(
                wTmp.c_str(),
                wDst.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == FALSE)
            {
                const DWORD e = ::GetLastError();

                // 6) 失敗時はtmpを掃除（掃除失敗は握りつぶす）
                ::DeleteFileW(wTmp.c_str());

                return make_fail_io(e, "MoveFileExW(replace) failed.");
            }
        }

        return make_ok();
    }

    std::string WinFileSystem::current_path() noexcept
    {
        // 1) 必要サイズ取得
        DWORD len = ::GetCurrentDirectoryW(0, nullptr);
        if (len == 0)
        {
            return {};
        }

        // 2) バッファ確保して取得
        std::wstring w;
        w.resize(static_cast<size_t>(len));

        const DWORD got = ::GetCurrentDirectoryW(len, w.data());
        if (got == 0)
        {
            return {};
        }

        // 3) 終端NULぶんを落とす
        if (!w.empty() && w.back() == L'\0')
        {
            w.pop_back();
        }

        // 4) UTF-8へ
        return wide_to_utf8(w);
    }
}
