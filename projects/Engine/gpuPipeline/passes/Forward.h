#pragma once

// === C++ Standard Library ===
#include <cstdint>

// === Engine ===
#include "Core/Error/Result.h"
#include "Engine/gpuPipeline/FrameGraph.h"

namespace Drama::Graphics
{
    namespace DX12
    {
        class ResourceManager;
    }

    class ForwardPass final : public FrameGraphPass
    {
    public:
        ForwardPass() = default;
        ~ForwardPass() override = default;
        const char* get_name() const override
        {
            // 1) PIX 表示用の名前を返す
            return "ForwardPass";
        }
        PassType get_pass_type() const override
        {
            // 1) GraphicsQueue で実行する
            return PassType::Render;
        }
        void setup_static(FrameGraphBuilder& builder) override
        {
            // 1) パス固有のリソース宣言を行う
            (void)builder;
        }
        void update_imports(FrameGraphBuilder& builder) override
        {
            // 1) パス固有のインポート更新を行う
            (void)builder;
        }
        void pipeline_requests(PipelineRequestCollector& outRequests) override
        {
            // 1) パス固有のパイプライン要求を追加する
            (void)outRequests;
        }
        void execute(FrameGraphContext& context) override
        {
            // 1) パス固有の描画コマンドを発行する
            (void)context;
        }
    private:
        // パス固有のメンバ変数をここに追加する
        Graphics::DX12::DescriptorAllocator& m_descriptorAllocator;


    };
}
