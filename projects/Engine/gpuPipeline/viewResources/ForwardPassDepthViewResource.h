#pragma once

// === C++ Standard Library ===
#include <array>
#include <cstdint>
#include <memory>

// === Windows ===
#include <dxgiformat.h>

// === Engine ===
#include "Engine/gpuPipeline/viewResources/ViewResource.h"

namespace Drama::Graphics
{
    class ForwardPassDepthViewResource final : public ViewResource
    {
    public:
        ForwardPassDepthViewResource() = default;
        ~ForwardPassDepthViewResource() override;

        void set_output_size(uint32_t width, uint32_t height)
        {
            // 1) 0 を許容しないため最低値を保証する
            // 2) 変更を再構築要求として記録する
            m_width = (width == 0) ? 1 : width;
            m_height = (height == 0) ? 1 : height;
            m_requestRebuild = true;
        }

        void set_depth_format(DXGI_FORMAT format)
        {
            // 1) フォーマットを保持して再構築要求を立てる
            m_depthFormat = format;
            m_requestRebuild = true;
        }

        void set_clear_depth(float depth)
        {
            // 1) クリア深度を保持して再構築要求を立てる
            m_clearDepth = depth;
            m_requestRebuild = true;
        }

        void set_clear_stencil(uint8_t stencil)
        {
            // 1) クリアステンシルを保持して再構築要求を立てる
            m_clearStencil = stencil;
            m_requestRebuild = true;
        }

        Core::Error::Result initialize(
            DX12::ResourceManager& resourceManager,
            uint32_t framesInFlight) override;

        void destroy() override;

        void add_passes(FrameGraph& frameGraph) override;

        ResourceHandle get_depth_handle() const
        {
            // 1) 現在の深度出力ハンドルを返す
            return m_depthHandle;
        }

        uint32_t get_width() const
        {
            // 1) 現在の幅を返す
            return m_width;
        }

        uint32_t get_height() const
        {
            // 1) 現在の高さを返す
            return m_height;
        }

        DXGI_FORMAT get_depth_format() const
        {
            // 1) 現在の深度形式を返す
            return m_depthFormat;
        }

    private:
        class SetupPass final : public FrameGraphPass
        {
        public:
            SetupPass(ForwardPassDepthViewResource& owner)
                : m_owner(owner)
            {
                // 1) 所有者参照を保持する
            }

            const char* get_name() const override
            {
                // 1) PIX 表示用の名前を返す
                return "ForwardPassDepthOutput";
            }

            PassType get_pass_type() const override
            {
                // 1) GraphicsQueue で実行する
                return PassType::Render;
            }

            void setup_static(FrameGraphBuilder& builder) override;
            void update_imports(FrameGraphBuilder& builder) override;
            void execute(FrameGraphContext& context) override;

        private:
            ForwardPassDepthViewResource& m_owner;
        };

    private:
        DX12::ResourceManager* m_resourceManager = nullptr;
        DX12::RenderDevice* m_renderDevice = nullptr;
        DX12::DescriptorAllocator* m_descriptorAllocator = nullptr;
        uint32_t m_framesInFlight = 1;

        uint32_t m_width = 1;
        uint32_t m_height = 1;
        DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        float m_clearDepth = 1.0f;
        uint8_t m_clearStencil = 0;
        bool m_requestRebuild = false;

        DX12::ComPtr<ID3D12Resource> m_depthResource;
        DX12::DescriptorAllocator::TableID m_depthDsvTable{};

        ResourceHandle m_depthHandle{};
        std::unique_ptr<SetupPass> m_setupPass;
    };
}
