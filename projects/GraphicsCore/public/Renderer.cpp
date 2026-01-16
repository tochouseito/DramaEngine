#include "pch.h"
#include "Renderer.h"
#include "RenderDevice.h"
#include "DescriptorAllocator.h"
#include "SwapChain.h"
#include "GraphicsConfig.h"

namespace Drama::Graphics::DX12
{
    Renderer::Renderer(RenderDevice& device, DescriptorAllocator& descriptorAllocator, SwapChain& swapChain)
        : m_renderDevice(device)
        , m_descriptorAllocator(descriptorAllocator)
        , m_swapChain(swapChain)
    {
        m_commandPool = std::make_unique<CommandPool>(m_renderDevice);
    }
    void Renderer::render(uint64_t frameNo, uint32_t index)
    {
        frameNo; index;
    }
    void Renderer::present(uint64_t frameNo, uint32_t index)
    {
        frameNo; index;
        // 1) コマンドコンテキストを取得する
        auto commandContext = m_commandPool->get_graphics_context();
        // 2) コマンドリストを取得する
        auto commandList = commandContext->get_command_list();
        // 3) コマンドリストの記録を開始する
        commandContext->reset();
        // 4) 描画コマンドを記録する
        // (TODO): 実際の描画コマンドをここに記録する
        commandList;
        // 5) コマンドリストの記録を終了する
        commandContext->close();
        // 6) コマンドリストの実行をキューに送る
        auto queueContext = m_renderDevice.get_queue_pool()->get_graphics_queue();;
        queueContext->execute(commandContext);
        // 7) GPUの処理が完了するまで待機する
        queueContext->wait_fence();
        // 8) キューコンテキストをプールに返す
        m_renderDevice.get_queue_pool()->return_queue(queueContext);
        // 9) コマンドコンテキストをプールに返す
        m_commandPool->return_context(commandContext);
        // 10) SwapChainのPresentを呼び出す
        auto presentQueue = m_renderDevice.get_queue_pool()->get_present_queue();
        if (graphicsConfig.m_enableVSync)
        {
            // VSync有効時
            m_swapChain->Present(1, 0);
        }
        else
        {
            // VSync無効時
            m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
        }
        // 11) Present用キューをflushする
        presentQueue->flush();
    }
} // namespace Drama::Graphics::DX12
