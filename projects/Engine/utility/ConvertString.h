#pragma once
// c++ standard library
#include <string>

// Windows
#include <Windows.h>

namespace Drama::Utility
{
    /// @brief UTF-8文字列をUTF-16文字列に変換します。
    /// @param utf8Str UTF-8文字列
    /// @return UTF-16文字列
    inline std::wstring ToUTF16(const std::string& utf8Str)
    {
        if (utf8Str.empty()) return {};
        int size = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8Str.c_str(), static_cast<int>(utf8Str.size()),
            nullptr, 0);
        if (size <= 0) return {};
        std::wstring ws(size, L'\0');
        ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            utf8Str.c_str(), static_cast<int>(utf8Str.size()),
            ws.data(), size);
        return ws;
    }
    /// @brief UTF-16文字列をUTF-8文字列に変換します。
    /// @param utf16Str UTF-16文字列
    /// @return UTF-8文字列
    inline std::string ToUTF8(const std::wstring& utf16Str)
    {
        if (utf16Str.empty()) return {};
        // 無効文字検出を有効化
        int size = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            utf16Str.c_str(), static_cast<int>(utf16Str.size()),
            nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string utf8(size, '\0');
        ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            utf16Str.c_str(), static_cast<int>(utf16Str.size()),
            utf8.data(), size, nullptr, nullptr);
        return utf8;
    }
}
