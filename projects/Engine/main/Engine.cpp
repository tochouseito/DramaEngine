#include "pch.h"
#include "Engine.h"
#include <Windows.h>
#include <wrl.h>

Drama::Engine::Engine() : m_Impl(std::make_unique<Impl>())
{
    // COM初期化
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

Drama::Engine::~Engine()
{
}

void Drama::Engine::Run()
{
}

bool Drama::Engine::Initialize()
{
    return false;
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
