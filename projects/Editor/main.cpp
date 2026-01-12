// === Windows library includes ===
#include <windows.h>

// === Drama Engine includes ===
#include "Engine/interface/EngineCreateAPI.h"

// === C++ standard library includes ===
#include <memory>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // 1) 生成した Engine を RAII で確実に破棄する
    // 2) 実行対象として Engine を登録して起動する
    std::unique_ptr<Drama::Engine, decltype(&Drama::API::destroy_engine)> engine(
        Drama::API::create_engine(), Drama::API::destroy_engine);
    Drama::API::set_engine(engine.get());

    Drama::API::run_engine();

    return 0;
}
