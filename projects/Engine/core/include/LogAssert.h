#pragma once
// C++ standard library includes
#include <string>
#include <format>
#include <Windows.h>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <deque>
#include <string>
#include <variant>
#include <mutex>
#include <system_error>
#include <source_location>
#ifdef _DEBUG
#include <debugapi.h>
#endif
// Drama Engine includes
#include "config/EngineConfig.h"

namespace Drama::Core
{
    class LogAssert final
    {
    public:
        using EXPR = std::variant<bool, HRESULT>;

        enum class LogLevel : uint8_t
        {

        };

        static void Init()
        {
            std::scoped_lock lock(m_Mutex);

            std::string currentDir = std::filesystem::current_path().string();
            m_LogPath = currentDir + "/" + EngineConfig::FilePath::EngineLogPath;

            std::error_code ec;
            std::filesystem::create_directories(m_LogPath.parent_path(), ec);
            if (ec) return;

            if (!std::filesystem::exists(m_LogPath, ec))
            {
                // 空ファイル作成
                std::ofstream ofs(m_LogPath, std::ios::binary);
                if (!ofs) return;
            }

            m_LineCount = CountLines_NoLock();

            // 起動時点で肥大化してたら整理
            if (m_LineCount > m_TrimTrigger)
            {
                TrimToLastN_NoLock(m_MaxLines);
            }
        }

        template <class... Args>
        static void Log(std::string_view fmt, Args&&... args)
        {
            std::string msg = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
#ifdef _DEBUG
            OutputDebugStringA(msg.c_str());
#endif
            WriteLine(msg);
        }

        static bool Check(EXPR expr, std::string_view fmt, std::source_location loc = std::source_location::current())
        {
            if (ToBool(expr))
            {
                return true;
            }
            const std::string msg = std::format("Check failed at {}:{} in {}: {}", loc.file_name(), loc.line(), loc.function_name(), fmt);

            WriteLine(msg);
#ifdef _DEBUG
            __debugbreak();
#endif
#ifndef NDEBUG
            MessageBoxA(nullptr, msg.c_str(), "Assertion Failed", MB_OK | MB_ICONERROR);
#endif
            return false;
        }

        template <class... Args>
        static void Throw(std::string_view fmt, Args&&... args)
        {
            const std::string msg = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
            WriteLine(msg);
#ifdef _DEBUG
            __debugbreak();
#endif
            throw std::runtime_error(msg);
        }

        static void Assert(EXPR expr, std::string_view fmt, std::source_location loc = std::source_location::current())
        {
            if(ToBool(expr))
            {
                return;
            }
            const std::string msg = std::format("Assert failed at {}:{} in {}: {}", loc.file_name(), loc.line(), loc.function_name(), fmt);

            WriteLine(msg);
#ifdef _DEBUG
            assert(false && msg.c_str());
#endif
#ifdef DEVELOP
            MessageBoxA(nullptr, msg.c_str(), "Assertion Failed", MB_OK | MB_ICONERROR);
#endif
            std::terminate();
        }

        // 1行追記（末尾に '\n' を付ける）
        static bool WriteLine(std::string_view line)
        {
            std::scoped_lock lock(m_Mutex);

            std::error_code ec;

            // 念のため（外部で消された場合など）
            std::filesystem::create_directories(m_LogPath.parent_path(), ec);
            if (ec) return false;

            {
                std::ofstream ofs(m_LogPath, std::ios::binary | std::ios::app);
                if (!ofs) return false;

                ofs << line.data() << "\n";
                if (!ofs) return false;
            }

            ++m_LineCount;

            if (m_LineCount > m_TrimTrigger)
            {
                if (!TrimToLastN_NoLock(m_MaxLines))
                {
                    // トリム失敗してもログ自体は書けてるので false にするかは好み
                    return false;
                }
            }

            return true;
        }
    private:

        static std::size_t CountLines_NoLock()
        {
            std::ifstream ifs(m_LogPath, std::ios::binary);
            if (!ifs) return 0;

            std::size_t count = 0;
            std::string s;
            while (std::getline(ifs, s)) ++count;
            return count;
        }

        static bool TrimToLastN_NoLock(std::size_t n)
        {
            std::ifstream ifs(m_LogPath, std::ios::binary);
            if (!ifs) return false;

            std::deque<std::string> last;
            last.clear();
            last.resize(0);

            std::string s;
            while (std::getline(ifs, s))
            {
                if (last.size() == n) last.pop_front();
                last.push_back(std::move(s));
            }

            {
                std::ofstream ofs(m_LogPath, std::ios::binary | std::ios::trunc);
                if (!ofs) return false;

                for (const auto& line : last)
                {
                    ofs << line << "\n";
                    if (!ofs) return false;
                }
            }

            m_LineCount = last.size();
            return true;
        }

        static bool ToBool(EXPR v) noexcept
        {
            return std::visit([](auto x) -> bool {
                if constexpr (std::is_same_v<std::decay_t<decltype(x)>, bool>)
                    return x;
                else
                    return SUCCEEDED(x); // HRESULT → bool
                }, v);
        }
    private:
        static inline std::filesystem::path m_LogPath = "temp/log.txt";
        static inline size_t m_MaxLines = 500;
        static inline size_t m_TrimTrigger = 550;
        static inline size_t m_LineCount = 0;
        static inline std::mutex m_Mutex;
    };
}
