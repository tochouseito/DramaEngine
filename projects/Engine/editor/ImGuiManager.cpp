#include "pch.h"
#include "ImGuiManager.h"
#ifndef NDEBUG
#include <filesystem>

#include <externals/imgui/include/imgui.h>
#include <externals/imgui/include/imgui_impl_win32.h>
#include <externals/imgui/include/imgui_impl_dx12.h>

#include "Core/IO/public/LogAssert.h"
#include "Engine/config/EngineConfig.h"
#include "Engine/gpuPipeline/FrameGraph.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/DescriptorAllocator.h"
#include "GraphicsCore/public/SwapChain.h"

namespace Drama::Editor
{
    namespace
    {
        class ImGuiPass final : public Graphics::FrameGraphPass
        {
        public:
            ImGuiPass(ImGuiManager& owner, Graphics::DX12::SwapChain& swapChain, Graphics::DX12::DescriptorAllocator& da)
                : m_owner(owner)
                , m_swapChain(swapChain)
                , m_descriptorAllocator(da)
            {
                // 1) 必要な参照を保持する
            }

            const char* get_name() const override
            {
                // 1) PIX 表示用の名前を返す
                return "ImGui";
            }
            Graphics::PassType get_pass_type() const override
            {
                // 1) GraphicsQueue で実行する
                return Graphics::PassType::Render;
            }
            void setup(Graphics::FrameGraphBuilder& builder) override
            {
                // 1) SwapChain のバックバッファを import して描画対象にする
                const uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
                Graphics::DX12::ComPtr<ID3D12Resource> backBuffer;
                const HRESULT hr = m_swapChain->GetBuffer(backBufferIndex, IID_PPV_ARGS(&backBuffer));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert_f(false, "Failed to get SwapChain back buffer.");
                    return;
                }

                m_backBuffer = backBuffer;
                m_backBufferHandle = builder.import_texture(
                    backBuffer.Get(),
                    D3D12_RESOURCE_STATE_PRESENT,
                    m_swapChain.get_rtv_table(backBufferIndex),
                    "ImGuiBackBuffer");
                builder.write_texture(
                    m_backBufferHandle,
                    D3D12_RESOURCE_STATE_RENDER_TARGET,
                    D3D12_RESOURCE_STATE_PRESENT);
            }
            void execute(Graphics::FrameGraphContext& context) override
            {
                // 1) コマンドリストを取得し、描画可能な状態を整える
                // 2) バックバッファへ ImGui を描画する
                ID3D12GraphicsCommandList* commandList = context.get_command_list();
                if (commandList == nullptr)
                {
                    return;
                }

                ID3D12DescriptorHeap* heaps[] = {
                    m_descriptorAllocator.get_descriptor_heap(Graphics::DX12::HeapType::CBV_SRV_UAV)
                };
                commandList->SetDescriptorHeaps(_countof(heaps), heaps);

                D3D12_VIEWPORT viewport{};
                viewport.TopLeftX = 0.0f;
                viewport.TopLeftY = 0.0f;
                viewport.Width = static_cast<float>(Graphics::g_graphicsConfig.m_screenWidth);
                viewport.Height = static_cast<float>(Graphics::g_graphicsConfig.m_screenHeight);
                viewport.MinDepth = 0.0f;
                viewport.MaxDepth = 1.0f;
                commandList->RSSetViewports(1, &viewport);

                D3D12_RECT scissorRect{};
                scissorRect.left = 0;
                scissorRect.top = 0;
                scissorRect.right = static_cast<LONG>(Graphics::g_graphicsConfig.m_screenWidth);
                scissorRect.bottom = static_cast<LONG>(Graphics::g_graphicsConfig.m_screenHeight);
                commandList->RSSetScissorRects(1, &scissorRect);

                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = context.get_rtv(m_backBufferHandle);
                commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);

                m_owner.Draw(commandList);
            }
        private:
            ImGuiManager& m_owner;
            Graphics::DX12::SwapChain& m_swapChain;
            Graphics::DX12::DescriptorAllocator& m_descriptorAllocator;
            Graphics::ResourceHandle m_backBufferHandle{};
            Graphics::DX12::ComPtr<ID3D12Resource> m_backBuffer;
        };
    }

    bool ImGuiManager::Initialize(EngineConfig& engineConfig, Graphics::GraphicsConfig& graphicsConfig, Graphics::DX12::RenderDevice& rdevice, Graphics::DX12::DescriptorAllocator& da, void* hwnd)
    {
        // 1) ImGui コンテキストを作成して基本設定を行う
        // 2) バックエンドとフォントを初期化して使用可能にする
        if (m_Initialized)
        {
            return true;
        }

        if (!hwnd)
        {
            Core::IO::LogAssert::assert_f(false, "ImGui initialize failed: hwnd is null.");
            return false;
        }

        IMGUI_CHECKVERSION();
        std::string version = IMGUI_VERSION;
        Core::IO::LogAssert::log("ImGui Version: {}", version);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Dockingを有効化
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // マルチビューポートを有効化
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // キーボードナビゲーションを有効化
        io.IniFilename = nullptr;
        errno_t err = ::strncpy_s(
            m_iniFilePath,
            sizeof(m_iniFilePath),
            engineConfig.m_imGuiIniPath.c_str(),
            _TRUNCATE);

        if (err != 0)
        {
            Core::IO::LogAssert::assert_f(false, "ImGui initialize failed: ini file path is too long.");
            return false;
        }

        LoadIni();

        ImGui_ImplWin32_Init(hwnd);

        m_descriptorAllocator = &da;
        m_fontTable = da.allocate(Graphics::DX12::DescriptorAllocator::TableKind::Textures);
        ID3D12DescriptorHeap* cbvSrvHeap = da.get_descriptor_heap(Graphics::DX12::HeapType::CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE imguiCpu = da.get_cpu_handle(m_fontTable);
        D3D12_GPU_DESCRIPTOR_HANDLE imguiGpu = da.get_gpu_handle(m_fontTable);

        ImGui_ImplDX12_Init(
            rdevice.get_d3d12_device(),
            graphicsConfig.m_bufferingCount,
            graphicsConfig.m_ldrOffscreenFormat,
            cbvSrvHeap,
            imguiCpu,
            imguiGpu);

        ImFontConfig fontConfig;
        fontConfig.MergeMode = false;
        fontConfig.PixelSnapH = true;
        static const ImWchar japaneseRanges[] = {
            0x0020, 0x00FF,  // ASCII
            0x3000, 0x30FF,  // 句読点・ひらがな・カタカナ
            0x4E00, 0x9FFF,  // 漢字
            0xFF00, 0xFFEF,  // 全角英数
            0,
        };
        io.Fonts->AddFontFromFileTTF(
            "packages/fonts/NotoSansJP-Regular.ttf",
            16.0f,
            &fontConfig,
            japaneseRanges);

        fontConfig.MergeMode = true;
        static const ImWchar iconRanges[] = { 0xf000, 0xf3ff, 0 };
        io.Fonts->AddFontFromFileTTF(
            "packages/fonts/Font Awesome 6 Free-Solid-900.otf",
            14.0f,
            &fontConfig,
            iconRanges);

        static const ImWchar materialSymbolRanges[] = { 0xe000, 0xf8ff, 0 };
        io.Fonts->AddFontFromFileTTF(
            "packages/fonts/MaterialSymbolsRounded-VariableFont_FILL,GRAD,opsz,wght.ttf",
            30.0f,
            &fontConfig,
            materialSymbolRanges);

        io.Fonts->Build();

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }
        style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesFull;

        m_Initialized = true;
        return true;
    }
    void ImGuiManager::Shutdown()
    {
        // 1) 初期化済みなら設定を保存して backend を終了する
        // 2) 確保したディスクリプタを解放して状態をリセットする
        if (!m_Initialized)
        {
            return;
        }

        SaveIni();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        if (m_descriptorAllocator && m_fontTable.valid())
        {
            m_descriptorAllocator->free_table(m_fontTable);
        }
        m_fontTable = {};
        m_descriptorAllocator = nullptr;
        m_Initialized = false;
    }
    void ImGuiManager::Begin()
    {
        // 1) 初期化済みの場合のみ新規フレームを開始する
        if (!m_Initialized)
        {
            return;
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }
    void ImGuiManager::End()
    {
        // 1) フレームを確定して draw data を生成する
        // 2) マルチビューポート対応のウィンドウを更新する
        if (!m_Initialized)
        {
            return;
        }

        ImGui::Render();

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }
    void ImGuiManager::Draw(ID3D12GraphicsCommandList* commandList)
    {
        // 1) 有効な draw data がある場合のみ描画する
        if (!m_Initialized || commandList == nullptr)
        {
            return;
        }

        ImDrawData* drawData = ImGui::GetDrawData();
        if (!drawData)
        {
            return;
        }

        ImGui_ImplDX12_RenderDrawData(drawData, commandList);
    }
    std::unique_ptr<Graphics::FrameGraphPass> ImGuiManager::CreatePass(
        Graphics::DX12::SwapChain& swapChain,
        Graphics::DX12::DescriptorAllocator& da)
    {
        // 1) ImGui 描画用の FrameGraphPass を生成して返す
        return std::make_unique<ImGuiPass>(*this, swapChain, da);
    }
    void ImGuiManager::SaveIni()
    {
        // 1) 初期化済みの場合のみ保存する
        // 2) 設定保存先のディレクトリを準備して保存する
        if (!m_Initialized)
        {
            return;
        }

        const std::filesystem::path iniPath = m_iniFilePath;
        if (iniPath.has_parent_path())
        {
            std::filesystem::create_directories(iniPath.parent_path());
        }
        ImGui::SaveIniSettingsToDisk(iniPath.string().c_str());
    }
    void ImGuiManager::LoadIni()
    {
        // 1) 既存の設定ファイルがあれば読み込む
        const std::filesystem::path iniPath = m_iniFilePath;
        if (!std::filesystem::exists(iniPath))
        {
            return;
        }
        ImGui::LoadIniSettingsFromDisk(iniPath.string().c_str());
    }
}

#endif
