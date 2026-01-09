#include "pch.h"
#include "public/Platform.h"
#include "windows/private/WinApp.h"
#include "windows/private/WinFileSystem.h"
#include "windows/private/WinLogger.h"
#include "windows/private/WinQpcClock.h"
#include "windows/private//WinWaiter.h"

namespace Drama::Platform
{
    struct System::Impl
    {
        std::unique_ptr<Win::WinApp> app;
        std::unique_ptr<Win::IO::WinFileSystem> fs;
        std::unique_ptr<Win::WinLogger> logger;
        std::unique_ptr<Win::Time::WinQpcClock> clock;
        std::unique_ptr<Win::Time::WinWaiter> waiter;
    };

    System::System() : m_impl(std::make_unique<Impl>())
    {
        m_impl->app = std::make_unique<Win::WinApp>();
        m_impl->fs = std::make_unique<Win::IO::WinFileSystem>();
        m_fs = m_impl->fs.get();
        m_impl->logger = std::make_unique<Win::WinLogger>();
        m_logger = m_impl->logger.get();
        m_impl->clock = std::make_unique<Win::Time::WinQpcClock>();
        m_clock = m_impl->clock.get();
        m_impl->waiter = std::make_unique<Win::Time::WinWaiter>(*m_clock);
        m_waiter = m_impl->waiter.get();
    }

    System::~System()
    {
    }

    bool System::init()
    {
        bool created = false;
        created = m_impl->app->create();
        m_impl->app->show();
        return created;
    }
    void System::update()
    {
    }
    void System::shutdown()
    {
        m_impl->app->shutdown();
    }
    bool System::pump_messages()
    {
        return m_impl->app->pump_messages();
    }
}
