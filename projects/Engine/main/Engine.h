#pragma once
// c++ standard library
#include <cstdint>
#include <memory>
#include <functional>

namespace Drama
{
    struct EngineConfig;

    namespace Platform
    {
        class System;
    }

    namespace Graphics
    {
        struct GraphicsConfig;
        class GpuPipeline;
        namespace DX12
        {
            class RenderDevice;
            class DescriptorAllocator;
            class CommandPool;
            class SwapChain;
        }
    }

    class Engine
    {
    public:
        using RenderCallback = std::function<void(uint64_t, uint32_t)>;
        using PostInitializeCallback = std::function<bool(Engine&)>;

        /// @brief コンストラクタ
        explicit Engine();
        /// @brief デストラクタ
        ~Engine();
        /// @brief 稼働
        void run();

        /// @brief 初期化後に呼ばれるコールバック設定
        void set_post_initialize_callback(PostInitializeCallback callback);
        /// @brief Render スレッドで呼ばれるコールバック設定
        void set_render_callback(RenderCallback callback);

        /// @brief Engine から利用中のインスタンスを取得する
        [[nodiscard]] EngineConfig& get_engine_config() const noexcept;
        [[nodiscard]] Graphics::GraphicsConfig& get_graphics_config() const noexcept;
        [[nodiscard]] Platform::System* get_platform() const noexcept;
        [[nodiscard]] Graphics::DX12::RenderDevice* get_render_device() const noexcept;
        [[nodiscard]] Graphics::DX12::DescriptorAllocator* get_descriptor_allocator() const noexcept;
        [[nodiscard]] Graphics::DX12::CommandPool* get_command_pool() const noexcept;
        [[nodiscard]] Graphics::DX12::SwapChain* get_swap_chain() const noexcept;
        [[nodiscard]] Graphics::GpuPipeline* get_gpu_pipeline() const noexcept;
    private:
        /// @brief 初期化
        /// @return 成功ならtrue
        [[nodiscard]]
        bool initialize();
        /// @brief 終了処理
        void shutdown();
        /// @brief 更新処理
        std::function<void(uint64_t, uint32_t)> update();
        /// @brief 描画処理
        std::function<void(uint64_t, uint32_t)> render();
        /// @brief フリップ処理
        std::function<void(uint64_t, uint32_t)> present();

        class Impl;
        std::unique_ptr<Impl> m_impl;
        bool m_isRunning = false;
    };
}
