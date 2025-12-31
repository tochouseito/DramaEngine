#include "pch.h"
#include "Engine.h"
#include <wrl.h>

// Drama Engine include
#include "Platform/Platform.h"
#include <core/include/LogAssert.h>

using namespace Drama;

class Drama::Engine::Impl
{
    friend class Engine;
public:
    Impl()
    {

    }
    ~Impl()
    {

    }
private:
    Platform::Windows windows;
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
        m_IsRunning = m_Impl->windows.PumpMessages();

        Update();
        Render();
    }
    // 終了処理
    Shutdown();
}

bool Drama::Engine::Initialize()
{
    // ウィンドウ作成
    if (!m_Impl->windows.Create())
    {
        return false;
    }
    // ウィンドウ表示
    m_Impl->windows.Show();

    return true;
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
