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
#include "GraphicsCore/public/DescriptorAllocator.h"
#include "GraphicsCore/public/ResourceManager.h"

namespace Drama
{
    class Drama::Engine::Impl
    {
        friend class Engine;
    public:
        Impl()
        {
            // 1) 生成直後は未初期化であることを明示するため空実装にする
        }
        ~Impl()
        {
            // 1) 所有リソースは unique_ptr の破棄に任せる
        }
    private:
        std::unique_ptr<Drama::Platform::System> m_platform = nullptr;
        std::unique_ptr<Drama::Core::Time::Clock> m_clock = nullptr;
        std::unique_ptr<Drama::Core::IO::Importer> m_importer = nullptr;
        std::unique_ptr<Drama::Core::IO::Exporter> m_exporter = nullptr;
        Drama::Frame::FramePipelineDesc m_framePipelineDesc{};
        std::unique_ptr<Drama::Frame::FramePipeline> m_framePipeline = nullptr;
        std::unique_ptr<Drama::Graphics::DX12::ResourceLeakChecker> m_resourceLeakChecker = nullptr;
        std::unique_ptr<Drama::Graphics::DX12::RenderDevice> m_renderDevice = nullptr;
        std::unique_ptr<Drama::Graphics::DX12::DescriptorAllocator> m_descriptorAllocator = nullptr;
        std::unique_ptr<Drama::Graphics::DX12::ResourceManager> m_resourceManager = nullptr;
    };

    Drama::Engine::Engine() : m_impl(std::make_unique<Impl>())
    {
        // 1) 依存リソースの初期化は run() 内に委譲する
    }

    Drama::Engine::~Engine()
    {
        // 1) 所有リソースの破棄は unique_ptr に任せる
    }

    void Drama::Engine::run()
    {
        // 1) 事前準備を整えて起動条件を確定させる
        m_isRunning = initialize();
        // 2) メインループ
        while (m_isRunning)
        {
            // 1) 終了要求を即時反映するためメッセージを処理する
            m_isRunning = m_impl->m_platform->pump_messages();

            // 2) 描画更新を継続するためフレームパイプラインを進める
            m_impl->m_framePipeline->step();

        }
        // 3) 終了時の後処理を行う
        shutdown();
    }

    bool Drama::Engine::initialize()
    {
        bool result = false;
        Core::Error::Result err;
        // 1) 依存する基盤を先に確立するため Platform を初期化する
        m_impl->m_platform = std::make_unique<Drama::Platform::System>();
        result = m_impl->m_platform->init();

        // 2) 以後の初期化で必要なサービスをまとめて取得する
        m_impl->m_clock = std::make_unique<Drama::Core::Time::Clock>(
            *m_impl->m_platform->clock());

        Core::IO::LogAssert::init(
            *m_impl->m_platform->fs(),
            *m_impl->m_platform->logger(),
            FilePath::engineLogPath);

        m_impl->m_importer = std::make_unique<Drama::Core::IO::Importer>(
            *m_impl->m_platform->fs());
        m_impl->m_exporter = std::make_unique<Drama::Core::IO::Exporter>(
            *m_impl->m_platform->fs());
        
        auto* threadFactory = m_impl->m_platform->thread_factory();
        auto* waiter = m_impl->m_platform->waiter();
        if (!threadFactory || !waiter)
        {
            return false;
        }

        // 3) 設定ファイルの存在を確認し、読み込み可能な状態にする
        // 3-1) 読み込み失敗時も起動できるよう NotFound を許容する
        Drama::EngineConfig engineConfig{};
        {
            err = m_impl->m_importer->import_engine_config(
                FilePath::engineConfigIniPath,
                engineConfig);
            if (!err && err.code != Core::Error::Code::NotFound)
            {
                return false;
            }
        }

        // 4) 更新/描画の流れを確立するためフレームパイプラインを生成する
        m_impl->m_framePipeline = std::make_unique<Drama::Frame::FramePipeline>(
            m_impl->m_framePipelineDesc,
            *threadFactory,
            *m_impl->m_clock,
            *waiter,
            update(),
            render(),
            present()
        );

        // 5) デバッグ時のリーク検知を有効にするため生成する
        m_impl->m_resourceLeakChecker = std::make_unique<Drama::Graphics::DX12::ResourceLeakChecker>();

        // 6) 描画の基盤を確立するため RenderDevice を生成する
        m_impl->m_renderDevice = std::make_unique<Drama::Graphics::DX12::RenderDevice>();
        err = m_impl->m_renderDevice->initialize(true);
        if (!err)
        {
            return false;
        }
        // 7) 描画で使うディスクリプタ管理を先に準備する
        m_impl->m_descriptorAllocator = std::make_unique<Drama::Graphics::DX12::DescriptorAllocator>(
            *m_impl->m_renderDevice);
        err = m_impl->m_descriptorAllocator->initialize(2048, 2048);
        if (!err)
        {
            return false;
        }
        // 8) 描画で使うリソース管理を先に準備する
        m_impl->m_resourceManager = std::make_unique<Drama::Graphics::DX12::ResourceManager>(
            *m_impl->m_renderDevice,
            *m_impl->m_descriptorAllocator,
            2048);
        return result;
    }

    void Drama::Engine::shutdown()
    {
        // 1) 実行中に更新された設定を永続化する
        {
            Drama::EngineConfig engineConfig{};
            Core::Error::Result result = m_impl->m_exporter->export_engine_config(
                FilePath::engineConfigIniPath,
                engineConfig);
            if (!result)
            {
                uint32_t code = static_cast<uint32_t>(result.code);
                Core::IO::LogAssert::log(
                    "Failed to export engine config. path={}, code={}",
                    FilePath::engineConfigIniPath,
                    code);
            }
        }
    }

    std::function<void(uint64_t, uint32_t)> Drama::Engine::update()
    {
        // 1) 更新処理のエントリポイントを返す
        // 2) 実装確定前でもパイプラインを動かすため仮実装にする
        return [this](uint64_t frameNo, uint32_t index)
            {
                (void)frameNo;
                (void)index;
                (void)this;
            };
    }

    std::function<void(uint64_t, uint32_t)> Drama::Engine::render()
    {
        // 1) 描画処理のエントリポイントを返す
        // 2) 実装確定前でもパイプラインを動かすため仮実装にする
        return [this](uint64_t frameNo, uint32_t index)
            {
                (void)frameNo;
                (void)index;
                (void)this;
            };
    }

    std::function<void(uint64_t, uint32_t)> Drama::Engine::present()
    {
        // 1) Present 処理のエントリポイントを返す
        // 2) 実装確定前でもパイプラインを動かすため仮実装にする
        return [this](uint64_t frameNo, uint32_t index)
            {
                (void)frameNo;
                (void)index;
                (void)this;
            };
    }

}
