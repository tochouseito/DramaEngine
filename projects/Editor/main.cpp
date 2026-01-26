// === Windows library includes ===
#include <windows.h>

// === Drama Engine includes ===
#include "Engine/interface/EngineCreateAPI.h"
#include "Platform/windows/public/WindowsNative.h"
#include "Editor/ImGuiManager.h"
#include "Engine/gpuPipeline/GpuPipeline.h"

// === C++ standard library includes ===
#include <memory>

// === ImGui includes ===
#include <externals/imgui/include/imgui.h>

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    // 1) 生成した Engine を RAII で確実に破棄する
    // 2) 実行対象として Engine を登録して起動する
    std::unique_ptr<Drama::Engine, decltype(&Drama::API::destroy_engine)> engine(
        Drama::API::create_engine(), Drama::API::destroy_engine);
    Drama::API::set_engine(engine.get());

    Drama::Editor::ImGuiManager imgui;

    engine->set_post_initialize_callback([&imgui](Drama::Engine& runtime) -> bool
        {
            // 1) Engine の内部インスタンスを取得して ImGui を初期化する
            // 2) ImGui 描画パスを登録する
            auto* renderDevice = runtime.get_render_device();
            auto* descriptorAllocator = runtime.get_descriptor_allocator();
            auto* swapChain = runtime.get_swap_chain();
            auto* gpuPipeline = runtime.get_gpu_pipeline();
            auto* platform = runtime.get_platform();
            if (!renderDevice || !descriptorAllocator || !swapChain || !gpuPipeline || !platform)
            {
                return false;
            }

            void* hwnd = Drama::Platform::Win::as_hwnd(*platform);
            if (!imgui.Initialize(runtime.get_engine_config(), runtime.get_graphics_config(), *renderDevice, *descriptorAllocator, hwnd))
            {
                return false;
            }

            gpuPipeline->register_pass(imgui.CreatePass(*swapChain, *descriptorAllocator));
            return true;
        });

    engine->set_render_callback([&imgui](uint64_t frameNo, uint32_t index)
        {
            // 1) ImGui のフレームを開始して UI を構築する
            (void)frameNo;
            (void)index;
            imgui.Begin();

            ImGui::Begin("Editor");
            ImGui::Text("ImGui is running.");
            ImGui::End();

            imgui.End();
        });

    Drama::API::run_engine();

    imgui.SaveIni();
    imgui.Shutdown();

    return 0;
}
