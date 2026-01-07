#include "pch.h"
#include "Engine.h"

// Drama Engine include
#include "Platform/public/Platform.h"
#include "Core/Time/FrameCounter.h"

class Drama::Engine::Impl
{
    friend class Engine;
public:
    Impl()
    {
        platform = std::make_unique<Drama::Platform::System>();
        clock = std::make_unique<Drama::Core::Time::Clock>(
            *platform->clock());
        frameCounter = std::make_unique<Drama::Core::Time::FrameCounter>(
            *clock.get());
    }
    ~Impl()
    {

    }
private:
    std::unique_ptr<Drama::Platform::System> platform = nullptr;
    std::unique_ptr<Drama::Core::Time::Clock> clock = nullptr;
    std::unique_ptr<Drama::Core::Time::FrameCounter> frameCounter = nullptr;
};

Drama::Engine::Engine() : m_Impl(std::make_unique<Impl>())
{
    
}

Drama::Engine::~Engine()
{
    
}

void Drama::Engine::Run()
{
    // 初期化
    m_IsRunning = Initialize();
    // メインループ
    while (m_IsRunning)
    {
        // ウィンドウメッセージ処理
        m_IsRunning = m_Impl->platform->pump_messages();

        Update();
        Render();
        m_Impl->frameCounter->tick();

    }
    // 終了処理
    Shutdown();
}

bool Drama::Engine::Initialize()
{
    bool result = false;

    result = m_Impl->platform->init();

    return result;
}

void Drama::Engine::Shutdown()
{
}

void Drama::Engine::Update()
{
}

void Drama::Engine::Render()
{
}
