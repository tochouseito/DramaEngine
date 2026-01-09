#include "pch.h"
#include "Engine.h"

// Drama Engine include
#include "Platform/public/Platform.h"
#include "frame/FramePipeline.h"

namespace Drama
{
    class Drama::Engine::Impl
    {
        friend class Engine;
    public:
        Impl()
        {
            platform = std::make_unique<Drama::Platform::System>();
            clock = std::make_unique<Drama::Core::Time::Clock>(
                *platform->clock());
        }
        ~Impl()
        {

        }
    private:
        std::unique_ptr<Drama::Platform::System> platform = nullptr;
        std::unique_ptr<Drama::Core::Time::Clock> clock = nullptr;
        Drama::Frame::FramePipelineDesc framePipelineDesc{};
        std::unique_ptr<Drama::Frame::FramePipeline> framePipeline = nullptr;
    };

    Drama::Engine::Engine() : m_Impl(std::make_unique<Impl>())
    {

    }

    Drama::Engine::~Engine()
    {

    }

    void Drama::Engine::Run()
    {
        // 1) 初期化
        m_IsRunning = Initialize();
        // 2) メインループ
        while (m_IsRunning)
        {
            // 1) ウィンドウメッセージ処理
            m_IsRunning = m_Impl->platform->pump_messages();

            // 2) フレームパイプライン処理
            m_Impl->framePipeline->step();

        }
        // 3) 終了処理
        Shutdown();
    }

    bool Drama::Engine::Initialize()
    {
        // 1) Platform 初期化
        bool result = false;

        result = m_Impl->platform->init();

        // 2) 依存インターフェイスを取得
        auto* threadFactory = m_Impl->platform->thread_factory();
        auto* waiter = m_Impl->platform->waiter();
        if (!threadFactory || !waiter)
        {
            return false;
        }

        // 3) フレームパイプラインを生成
        m_Impl->framePipeline = std::make_unique<Drama::Frame::FramePipeline>(
            m_Impl->framePipelineDesc,
            *threadFactory,
            *m_Impl->clock,
            *waiter,
            Update(),
            Render(),
            Present()
        );

        return result;
    }

    void Drama::Engine::Shutdown()
    {
    }

    std::function<void(uint64_t, uint32_t)> Drama::Engine::Update()
    {
        // 1) 更新処理のエントリポイントを返す
        // 2) 実装が確定するまでの仮実装
        return [this](uint64_t frameNo, uint32_t index)
            {
                (void)frameNo;
                (void)index;
                (void)this;
            };
    }

    std::function<void(uint64_t, uint32_t)> Drama::Engine::Render()
    {
        // 1) 描画処理のエントリポイントを返す
        // 2) 実装が確定するまでの仮実装
        return [this](uint64_t frameNo, uint32_t index)
            {
                (void)frameNo;
                (void)index;
                (void)this;
            };
    }

    std::function<void(uint64_t, uint32_t)> Drama::Engine::Present()
    {
        // 1) Present 処理のエントリポイントを返す
        // 2) 実装が確定するまでの仮実装
        return [this](uint64_t frameNo, uint32_t index)
            {
                (void)frameNo;
                (void)index;
                (void)this;
            };
    }

}
