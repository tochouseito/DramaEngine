#include "ImGuiManager.h"

#include <filesystem>

#include <externals/imgui/include/imgui.h>
#include <externals/imgui/include/imgui_impl_win32.h>
#include <externals/imgui/include/imgui_impl_dx12.h>

#include "Core/IO/public/LogAssert.h"
#include "Engine/config/EngineConfig.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/DescriptorAllocator.h"

namespace Drama::Editor
{
    bool ImGuiManager::Initialize(Graphics::DX12::RenderDevice& rdevice, Graphics::DX12::DescriptorAllocator& da)
    {
        // imgui バージョン表示
        IMGUI_CHECKVERSION();
        std::string version = IMGUI_VERSION;
        Core::IO::LogAssert::log("ImGui Version: {}", version);
        // コンテキストの作成
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();/*
        io.DisplaySize = ImVec2(
            static_cast<float>(WinApp::m_WindowWidth),
            static_cast<float>(WinApp::m_WindowHeight));*/
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Dockingを有効化
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // マルチビューポートを有効化
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // キーボードナビゲーションを有効化
        // io.IniFilename = nullptr; // 設定ファイルを無効化
        // 設定ファイルの読み込み
        LoadIni();
        // プラットフォームのバックエンドを設定する
        ImGui_ImplWin32_Init(WinApp::m_HWND);
        // レンダラーのバックエンドを設定する
        DescriptorAllocator::TableID tableId = da.Allocate(DescriptorAllocator::TableKind::Textures);
        ID3D12DescriptorHeap* cbvSrvHeap = da.GetDescriptorHeap(HeapType::CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE imguiCpu =
            cbvSrvHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE imguiGpu =
            cbvSrvHeap->GetGPUDescriptorHandleForHeapStart();

        ImGui_ImplDX12_Init(
            rdevice.get_d3d12_device(),
            Graphics::graphicsConfig.m_bufferingCount,
            Graphics::graphicsConfig.m_ldrOffscreenFormat,
            cbvSrvHeap, // ★ GPU 可視ヒープ
            imguiCpu,           // ★ 同じヒープの CPU ハンドル
            imguiGpu            // ★ 同じヒープの GPU ハンドル
        );

        // フォントの設定
        ImFontConfig font_config;
        font_config.MergeMode = false;
        font_config.PixelSnapH = true;
        static const ImWchar japaneseRanges[] = {
        0x0020, 0x00FF,  // ASCII
        0x3000, 0x30FF,  // 句読点・ひらがな・カタカナ
        0x4E00, 0x9FFF,  // 漢字
        0xFF00, 0xFFEF,  // 全角英数
        0,
        };
        io.Fonts->AddFontFromFileTTF(
            "packages/fonts/NotoSansJP-Regular.ttf",// フォントファイルのパス
            16.0f,// フォントファイルのパスとフォントサイズ
            &font_config,
            japaneseRanges
        );
        // アイコンフォントをマージ
        font_config.MergeMode = true;
        static const ImWchar icon_ranges[] = { 0xf000, 0xf3ff, 0 }; // FontAwesomeの範囲
        io.Fonts->AddFontFromFileTTF(
            "packages/fonts/Font Awesome 6 Free-Solid-900.otf",
            14.0f,
            &font_config, icon_ranges);
        // Material Symbols Rounded をマージ
        static const ImWchar materialSymbolRanges[] = { 0xe000, 0xf8ff, 0 }; // Private Use Area
        io.Fonts->AddFontFromFileTTF(
            "packages/fonts/MaterialSymbolsRounded-VariableFont_FILL,GRAD,opsz,wght.ttf",
            30.0f,
            &font_config,
            materialSymbolRanges
        );
        // 標準フォントを追加する
        io.Fonts->Build(); // フォントをビルド

        // ImGui スタイルの設定
        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        // ビューポートが有効な場合、ウィンドウの角を丸くしない
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        // ツリーラインの表示
        style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesFull;
        m_Initialized = true;
        return true;
    }
    void ImGuiManager::Shutdown()
    {
    }
    void ImGuiManager::Begin()
    {
    }
    void ImGuiManager::End()
    {
    }
    void ImGuiManager::Draw(Graphics::DX12::CommandContext& ctx)
    {
        ctx;
    }
    void ImGuiManager::SaveIni()
    {
    }
    void ImGuiManager::LoadIni()
    {
    }
}
