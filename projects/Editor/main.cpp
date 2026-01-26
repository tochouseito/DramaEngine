// === Windows library includes ===
#include <windows.h>

// === Drama Engine includes ===
#include "Engine/interface/EngineCreateAPI.h"
#include "Platform/windows/public/WindowsNative.h"
#include "Engine/gpuPipeline/GpuPipeline.h"

// === C++ standard library includes ===
#include <memory>

// === ImGui includes ===
#ifndef NDEBUG
#include <externals/imgui/include/imgui.h>
#endif

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // 1) 生成した Engine を RAII で確実に破棄する
    // 2) 実行対象として Engine を登録して起動する
    std::unique_ptr<Drama::Engine, decltype(&Drama::API::destroy_engine)> engine(
        Drama::API::create_engine(), Drama::API::destroy_engine);
    Drama::API::set_engine(engine.get());

#ifndef NDEBUG
    Drama::API::set_render_callback([](uint64_t frameNo, uint32_t index)
        {
            // 1) ImGui のフレームを開始して UI を構築する
            (void)frameNo;
            (void)index;
            ImGui::Begin("Editor");
            ImGui::Text("ImGui is running.");
            ImGui::End();
        });
#endif

    Drama::API::run_engine();

    return 0;
}
