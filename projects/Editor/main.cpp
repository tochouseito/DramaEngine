// === Windows library includes ===
#include <windows.h>

// === Drama Engine includes ===
#include "Engine/utility/EngineCreateAPI.h"

// === C++ standard library includes ===
#include <memory>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // Engineの生成
    std::unique_ptr<Drama::Engine, decltype(&Drama::API::DestroyEngine)> engine(
        Drama::API::CreateEngine(), Drama::API::DestroyEngine);// エンジンの生成
    Drama::API::SetEngine(engine.get());// エンジンのポインタをセット

    Drama::API::RunEngine();

    return 0;
}
