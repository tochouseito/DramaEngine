#pragma once
// C++ standard library includes
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <cstddef>
#include <cstdint>
#include "Core/include/IFileSystem.h"

namespace Drama::Core
{
    class LogAssert final
    {
    public:
        static bool Init(IO::IFileSystem& fs, std::string logPathUtf8,
            size_t maxLines = 500, size_t trimTrigger = 550)
        {
            std::scoped_lock lock(m_Mutex);

            m_Fs = &fs;
            m_LogPath = std::move(logPathUtf8);
            m_MaxLines = maxLines;
            m_TrimTrigger = trimTrigger;

            if (!EnsureParentDir_NoLock())
            {
                return false;
            }
            if (!EnsureFileExists_NoLock())
            {
                return false;
            }

            m_LineCount = CountLines_NoLock();

            if (m_LineCount > m_TrimTrigger)
            {
                if (!TrimToLastN_NoLock(m_MaxLines))
                {
                    return false;
                }
            }
            return true;
        }

        static bool WriteLine(std::string_view lineUtf8) noexcept
        {
            std::scoped_lock lock(m_Mutex);

            if (!m_Fs)
            {
                return false; // Init忘れ
            }

            if (!EnsureParentDir_NoLock())
            {
                return false;
            }

            // string_viewは終端'\0'保証なし。必ず(data,size)で追記する。
            m_Tmp.clear();
            m_Tmp.reserve(lineUtf8.size() + 1);
            m_Tmp.append(lineUtf8.data(), lineUtf8.size());
            m_Tmp.push_back('\n');

            {
                auto r = m_Fs->AppendAllBytes(m_LogPath, m_Tmp.data(), m_Tmp.size());
                if (!r)
                    return false;
            }

            ++m_LineCount;

            if (m_LineCount > m_TrimTrigger)
            {
                if (!TrimToLastN_NoLock(m_MaxLines))
                {
                    // トリム失敗でもログ追記自体は成功している。
                    // ここを false にするかは運用次第。今はあなたの元コードに合わせて false。
                    return false;
                }
            }

            return true;
        }

    private:
        static std::string ParentDirUtf8(std::string_view path)
        {
            const size_t p = path.find_last_of("/\\");
            if (p == std::string_view::npos)
            {
                return {};
            }
            return std::string(path.substr(0, p));
        }

        static bool EnsureParentDir_NoLock() noexcept
        {
            const std::string parent = ParentDirUtf8(m_LogPath);
            if (parent.empty())
            {
                return true;
            }

            auto r = m_Fs->CreateDirectories(parent);
            return static_cast<bool>(r);
        }

        static bool EnsureFileExists_NoLock() noexcept
        {
            // あなたのExists()仕様に合わせる：
            // OKなら存在、NotFoundなら無い、それ以外はエラー
            auto r = m_Fs->Exists(m_LogPath);
            if (r)
            {
                return true;
            }

            if (r.error == IO::FsError::NotFound)
            {
                // 空ファイル作成
                auto r2 = m_Fs->WriteAllBytes(m_LogPath, "", 0);
                return static_cast<bool>(r2);
            }
            return false;
        }

        static size_t CountLines_NoLock() noexcept
        {
            std::vector<uint8_t> bytes;
            auto r = m_Fs->ReadAllBytes(m_LogPath, bytes);
            if (!r)
            {
                return 0;
            }

            size_t count = 0;
            for (uint8_t c : bytes)
            {
                if (c == '\n')
                {
                    ++count;
                }
            }

            return count;
        }

        static bool TrimToLastN_NoLock(size_t n) noexcept
        {
            std::vector<uint8_t> bytes;
            auto r = m_Fs->ReadAllBytes(m_LogPath, bytes);
            if (!r)
            {
                return false;
            }

            // 行数カウント（\n基準）
            size_t lines = 0;
            for (uint8_t c : bytes)
                if (c == '\n')
                    ++lines;

            if (lines <= n)
            {
                m_LineCount = lines;
                return true;
            }

            // 後ろから n 行分の開始位置を探す
            size_t need = n; // 末尾から数える\nの数
            size_t i = bytes.size();

            while (i > 0 && need > 0)
            {
                --i;
                if (bytes[i] == '\n')
                {
                    --need;
                }
            }

            // i は「n行分の先頭の直前 or ちょうど先頭付近」
            // ここでは i+1 を開始にすると、「ちょうど境界の次」から取れる。
            size_t start = (i + 1 < bytes.size()) ? (i + 1) : 0;

            const uint8_t* p = bytes.data() + start;
            const size_t   sz = bytes.size() - start;

            r = m_Fs->WriteAllBytes(m_LogPath, p, sz);
            if (!r)
            {
                return false;
            }

            m_LineCount = n;
            return true;
        }

    private:
        static inline IO::IFileSystem* m_Fs = nullptr;
        static inline std::string m_LogPath = "temp/log.txt";

        static inline size_t m_MaxLines = 500;
        static inline size_t m_TrimTrigger = 550;
        static inline size_t m_LineCount = 0;

        static inline std::mutex m_Mutex;

        // 使い回しバッファ（毎回newしない）
        static inline std::string m_Tmp;
    };
}
