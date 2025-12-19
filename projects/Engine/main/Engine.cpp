#include "pch.h"
#include "Engine.h"
#include <Windows.h>
#include <wrl.h>

// Drama Engine include
#include <platform/include/WinApp.h>

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

};

Drama::Engine::Engine() : m_Impl(std::make_unique<Impl>())
{
    // COM初期化
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        // 初期化失敗
        
    }
}

Drama::Engine::~Engine()
{
    // COM終了処理
    CoUninitialize();
}

void Drama::Engine::Run()
{
    // 初期化
    m_IsRunning = Initialize();
    // メインループ
    while (m_IsRunning)
    {
        // ウィンドウメッセージ処理
        m_IsRunning = Platform::WinApp::ProcessMessage();

        Update();
        Render();
    }
    // 終了処理
    Shutdown();
}

bool Drama::Engine::Initialize()
{
    // ウィンドウ作成
    if (!Platform::WinApp::CreateWindowApp())
    {
        return false;
    }
    // ウィンドウ表示
    Platform::WinApp::ShowWindowApp();

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
