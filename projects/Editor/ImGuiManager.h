#pragma once

namespace Drama::Graphics::DX12
{
    class RenderDevice;
    class DescriptorAllocator;
    class CommandContext;
}

namespace Drama::Editor
{
    class ImGuiManager final
    {
    public:
        bool Initialize(Graphics::DX12::RenderDevice& rdevice, Graphics::DX12::DescriptorAllocator& da);
        void Shutdown();
        void Begin();
        void End();
        void Draw(Graphics::DX12::CommandContext& ctx);
        void SaveIni();
        void LoadIni();
    private:
        bool m_Initialized = false;
    };
}
