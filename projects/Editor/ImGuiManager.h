#pragma once
#include <memory>

#include "Engine/config/EngineConfig.h"
#include "GraphicsCore/public/DescriptorAllocator.h"
#include "GraphicsCore/public/GraphicsConfig.h"

namespace Drama::Graphics::DX12
{
    class RenderDevice;
    class DescriptorAllocator;
    class SwapChain;
}

namespace Drama::Graphics
{
    class FrameGraphPass;
}

struct ID3D12GraphicsCommandList;

namespace Drama::Editor
{
    class ImGuiManager final
    {
    public:
        bool Initialize(EngineConfig& _engineConfig, Graphics::GraphicsConfig& _graphicsConfig, Graphics::DX12::RenderDevice& rdevice, Graphics::DX12::DescriptorAllocator& da, void* hwnd);
        void Shutdown();
        void Begin();
        void End();
        void Draw(ID3D12GraphicsCommandList* commandList);
        std::unique_ptr<Graphics::FrameGraphPass> CreatePass(
            Graphics::DX12::SwapChain& swapChain,
            Graphics::DX12::DescriptorAllocator& da);
        void SaveIni();
        void LoadIni();
    private:
        bool m_Initialized = false;
        std::string m_iniFilePath;
        Graphics::DX12::DescriptorAllocator* m_descriptorAllocator = nullptr;
        Graphics::DX12::DescriptorAllocator::TableID m_fontTable{};
    };
}
