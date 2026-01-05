#include "pch.h"
#include "Engine.h"

// Drama Engine include
#include "Platform/public/Platform.h"

class Drama::Engine::Impl
{
    friend class Engine;
public:
    Impl()
    {
        platform = std::make_unique<Drama::Platform::System>();
    }
    ~Impl()
    {

    }
private:
    std::unique_ptr<Drama::Platform::System> platform = nullptr;
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
