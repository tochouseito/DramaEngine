#pragma once
#include "Core/IO/public/IFileSystem.h"
#include "Core/IO/public/ILogger.h"
#include "Core/Time/IMonotonicClock.h"
#include "Core/Time/IWaiter.h"

#include <memory>

namespace Drama::Platform
{
    enum class Message
    {

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
    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;

        Core::IO::IFileSystem* m_fs = nullptr;
        Core::IO::ILogger* m_logger = nullptr;
        Core::Time::IMonotonicClock* m_clock = nullptr;
        Core::Time::IWaiter* m_waiter = nullptr;
    };
}
