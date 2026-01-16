#pragma once
#include "Core/IO/public/IFileSystem.h"
#include "Core/IO/public/ILogger.h"
#include "Core/Time/IMonotonicClock.h"
#include "Core/Time/IWaiter.h"
#include "Core/Threading/IThreadFactory.h"

#include <memory>

namespace Drama::Platform
{
    class System;

    namespace Win
    {
        void* as_hwnd(const System& sys) noexcept;
    }

    enum class Message
    {

    };

    struct AppInfo
    {
        uint32_t m_width = 1280;
        uint32_t m_height = 720;
    };

    class System final
    {
    public:
        System();
        ~System();

        [[nodiscard]] bool init();
        void update();
        void shutdown();

        [[nodiscard]] bool pump_messages();

        Core::IO::IFileSystem* fs() noexcept
        {
            return m_fs;
        }
        Core::IO::ILogger* logger() noexcept
        {
            return m_logger;
        }
        Core::Time::IMonotonicClock* clock() noexcept
        {
            return m_clock;
        }
        Core::Time::IWaiter* waiter() noexcept
        {
            return m_waiter;
        }
        Core::Threading::IThreadFactory* thread_factory() noexcept
        {
            return m_threadFactory;
        }
        
        [[nodiscard]] const AppInfo& app_info() noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
        friend void* Win::as_hwnd(const System& sys) noexcept;

        AppInfo m_appInfo;

        Core::IO::IFileSystem* m_fs = nullptr;
        Core::IO::ILogger* m_logger = nullptr;
        Core::Time::IMonotonicClock* m_clock = nullptr;
        Core::Time::IWaiter* m_waiter = nullptr;
        Core::Threading::IThreadFactory* m_threadFactory = nullptr;
    };
}
