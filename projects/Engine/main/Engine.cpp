#include "pch.h"
#include "Engine.h"

// Drama Engine include
#include "config/EngineConfig.h"
#include "Platform/public/Platform.h"
#include "Core/Error/Result.h"
#include "Core/IO/public/LogAssert.h"
#include "Core/IO/public/Importer.h"
#include "Core/IO/public/Exporter.h"
#include "frame/FramePipeline.h"
#include "GraphicsCore/public/ResourceLeakChecker.h"
#include "GraphicsCore/public/RenderDevice.h"

namespace Drama
{
    class Drama::Engine::Impl
    {
        friend class Engine;
    public:
        Impl()
        {
        }
        ~Impl()
        {

        }
    private:
        std::unique_ptr<Drama::Platform::System> platform = nullptr;
        std::unique_ptr<Drama::Core::Time::Clock> clock = nullptr;
        std::unique_ptr<Drama::Core::IO::Importer> importer = nullptr;
        std::unique_ptr<Drama::Core::IO::Exporter> exporter = nullptr;
        Drama::Frame::FramePipelineDesc framePipelineDesc{};
        std::unique_ptr<Drama::Frame::FramePipeline> framePipeline = nullptr;
        std::unique_ptr<Drama::Graphics::DX12::ResourceLeakChecker> resourceLeakChecker = nullptr;
        std::unique_ptr<Drama::Graphics::DX12::RenderDevice> renderDevice = nullptr;
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
        bool result = false;
        Core::Error::Result err;
        // 1) Platform 初期化
        m_Impl->platform = std::make_unique<Drama::Platform::System>();
        result = m_Impl->platform->init();

        // 2) 依存インターフェイスを取得
        m_Impl->clock = std::make_unique<Drama::Core::Time::Clock>(
            *m_Impl->platform->clock());

        Core::IO::LogAssert::init(
            *m_Impl->platform->fs(),
            *m_Impl->platform->logger(),
            FilePath::Engine_Log_Path);

        m_Impl->importer = std::make_unique<Drama::Core::IO::Importer>(
            *m_Impl->platform->fs());
        m_Impl->exporter = std::make_unique<Drama::Core::IO::Exporter>(
            *m_Impl->platform->fs());
        
        auto* threadFactory = m_Impl->platform->thread_factory();
        auto* waiter = m_Impl->platform->waiter();
        if (!threadFactory || !waiter)
        {
            return false;
        }

        // 3) Config を設定
        // 3-1) EngineConfig を読み込む
        Drama::EngineConfig engineConfig{};
        {
            err = m_Impl->importer->import_engine_config(
                FilePath::Engine_Config_iniPath,
                engineConfig);
            if (!err && err.code != Core::Error::Code::NotFound)
            {
                return false;
            }
        }

        // 4) フレームパイプラインを生成
        m_Impl->framePipeline = std::make_unique<Drama::Frame::FramePipeline>(
            m_Impl->framePipelineDesc,
            *threadFactory,
            *m_Impl->clock,
            *waiter,
            Update(),
            Render(),
            Present()
        );

        // 5) リソースリークチェッカーを生成
        m_Impl->resourceLeakChecker = std::make_unique<Drama::Graphics::DX12::ResourceLeakChecker>();

        // 6) RenderDevice を生成
        m_Impl->renderDevice = std::make_unique<Drama::Graphics::DX12::RenderDevice>();
        err = m_Impl->renderDevice->initialize(true);
        if (!err)
        {
            return false;
        }

        return result;
    }

    void Drama::Engine::Shutdown()
    {
        // Configの保存
        {
            Drama::EngineConfig engineConfig{};
            Core::Error::Result result = m_Impl->exporter->export_engine_config(
                FilePath::Engine_Config_iniPath,
                engineConfig);
            if (!result)
            {
                uint32_t code = static_cast<uint32_t>(result.code);
                Core::IO::LogAssert::log(
                    "Failed to export engine config. path={}, code={}",
                    FilePath::Engine_Config_iniPath,
                    code);
            }
        }
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
