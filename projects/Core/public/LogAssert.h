#pragma once
// C++ standard library includes
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <format>
#include <variant>
#include <source_location>
#include <vector>
#include "Core/public/IFileSystem.h"
#include "Core/public/ILogger.h"

namespace Drama::Core
{
    class LogAssert final
    {
        using EXPR = std::variant<bool, Error::Result>; 
    public:
        /// @brief ログファイルを初期化する
        /// @return 成功ならtrue、失敗ならfalse
        static bool init(IO::IFileSystem& fs, IO::ILogger& log, std::string logPathUtf8,
            size_t maxLines = 500, size_t trimTrigger = 550)
        {
            std::scoped_lock lock(m_mutex);

            // 1) 初期状態をセットアップする
            m_fs = &fs;
            m_log = &log;
            m_logPath = std::move(logPathUtf8);
            m_maxLines = maxLines;
            m_trimTrigger = trimTrigger;

            // 2) ログ出力先を準備する
            if (!ensure_parent_dir_no_lock())
            {
                return false;
            }
            if (!ensure_file_exists_no_lock())
            {
                return false;
            }

            // 3) 行数を初期化し、必要ならトリムする
            m_lineCount = count_lines_no_lock();

            if (m_lineCount > m_trimTrigger)
            {
                if (!trim_to_last_n_no_lock(m_maxLines))
                {
                    return false;
                }
            }
            return true;
        }

        /// @brief ログ出力
        template <class... Args>
        static void log(std::string_view fmt, Args&&... args)
        {
            std::string msg = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
#ifdef _DEBUG
            m_log->output_debug_string(msg);
#endif
            write_line(msg);
        }

        /// @brief 条件チェック。失敗時にログ出力。

    private:
        /// @brief 1行追記する（末尾に'\n'を付ける）
        /// @return 成功ならtrue、失敗ならfalse
        static bool write_line(std::string_view lineUtf8) noexcept
        {
            std::scoped_lock lock(m_mutex);

            // 1) 初期化済みであることを確認する
            if (!m_fs)
            {
                return false; // init忘れ
            }

            // 2) 出力先を準備する
            if (!ensure_parent_dir_no_lock())
            {
                return false;
            }

            // 3) バッファを整形して追記する
            // string_viewは終端'\0'保証なし。必ず(data,size)で追記する。
            m_tmp.clear();
            m_tmp.reserve(lineUtf8.size() + 1);
            m_tmp.append(lineUtf8.data(), lineUtf8.size());
            m_tmp.push_back('\n');

            {
                auto r = m_fs->append_all_bytes(m_logPath, m_tmp.data(), m_tmp.size());
                if (!r)
                {
                    return false;
                }
            }

            ++m_lineCount;

            if (m_lineCount > m_trimTrigger)
            {
                if (!trim_to_last_n_no_lock(m_maxLines))
                {
                    // トリム失敗でもログ追記自体は成功している。
                    // ここを false にするかは運用次第。今はあなたの元コードに合わせて false。
                    return false;
                }
            }

            return true;
        }

        static std::string parent_dir_utf8(std::string_view path)
        {
            // 1) 区切り位置を探す
            const size_t pos = path.find_last_of("/\\");
            if (pos == std::string_view::npos)
            {
                return {};
            }
            // 2) 親ディレクトリを返す
            return std::string(path.substr(0, pos));
        }

        static bool ensure_parent_dir_no_lock() noexcept
        {
            // 1) 親ディレクトリを取得する
            const std::string parentDir = parent_dir_utf8(m_logPath);
            if (parentDir.empty())
            {
                return true;
            }

            // 2) 必要なら作成する
            auto r = m_fs->create_directories(parentDir);
            return static_cast<bool>(r);
        }

        static bool ensure_file_exists_no_lock() noexcept
        {
            // 1) 既存ファイルかどうか判定する
            // あなたのexists()仕様に合わせる：
            // OKなら存在、NotFoundなら無い、それ以外はエラー
            auto r = m_fs->exists(m_logPath);
            if (r)
            {
                return true;
            }

            // 2) 無ければ空ファイルを作成する
            if (r.code == Error::Code::NotFound)
            {
                // 空ファイル作成
                auto r2 = m_fs->write_all_bytes(m_logPath, "", 0);
                return static_cast<bool>(r2);
            }
            return false;
        }

        static size_t count_lines_no_lock() noexcept
        {
            // 1) 全内容を読み込む
            std::vector<uint8_t> bytes;
            auto r = m_fs->read_all_bytes(m_logPath, bytes);
            if (!r)
            {
                return 0;
            }

            // 2) 改行数を数える
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

        static bool trim_to_last_n_no_lock(size_t n) noexcept
        {
            // 1) 全内容を読み込む
            std::vector<uint8_t> bytes;
            auto r = m_fs->read_all_bytes(m_logPath, bytes);
            if (!r)
            {
                return false;
            }

            // 2) 行数を数えて必要性を判断する
            // 行数カウント（\n基準）
            size_t lines = 0;
            for (uint8_t c : bytes)
            {
                if (c == '\n')
                {
                    ++lines;
                }
            }

            if (lines <= n)
            {
                m_lineCount = lines;
                return true;
            }

            // 3) 後ろから必要行数を探す
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

            // 4) 末尾からn行分を保存し直す
            const uint8_t* p = bytes.data() + start;
            const size_t size = bytes.size() - start;

            r = m_fs->write_all_bytes(m_logPath, p, size);
            if (!r)
            {
                return false;
            }

            m_lineCount = n;
            return true;
        }

    private:
        static inline IO::IFileSystem* m_fs = nullptr;
        static inline IO::ILogger* m_log = nullptr;
        static inline std::string m_logPath = "temp/log.txt";

        static inline size_t m_maxLines = 500;
        static inline size_t m_trimTrigger = 550;
        static inline size_t m_lineCount = 0;

        static inline std::mutex m_mutex;

        // 使い回しバッファ（毎回newしない）
        static inline std::string m_tmp;
    };
}
