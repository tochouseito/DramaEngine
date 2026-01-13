#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Drama::Platform::Win
{
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
}
