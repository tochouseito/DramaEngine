#include "pch.h"
#include "Renderer.h"
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/DescriptorAllocator.h"
#include "GraphicsCore/public/SwapChain.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics
{
    Renderer::Renderer(DX12::RenderDevice& device, DX12::DescriptorAllocator& descriptorAllocator, DX12::SwapChain& swapChain)
        : m_renderDevice(device)
        , m_descriptorAllocator(descriptorAllocator)
        , m_swapChain(swapChain)
    {
        m_commandPool = std::make_unique<DX12::CommandPool>(m_renderDevice);
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
        // ディスクリプタヒープをセットする
        ID3D12DescriptorHeap* heaps[] = {
            m_descriptorAllocator.get_descriptor_heap(DX12::HeapType::CBV_SRV_UAV)
        };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);
        // ビューポートとシザー矩形の設定
        D3D12_VIEWPORT viewport{
            0.0f, 0.0f,
            static_cast<float>(graphicsConfig.m_screenWidth),
            static_cast<float>(graphicsConfig.m_screenHeight),
            0.0f, 1.0f
        };
        commandList->RSSetViewports(1, &viewport);
        D3D12_RECT scissorRect{
            0, 0,
            static_cast<LONG>(graphicsConfig.m_screenWidth),
            static_cast<LONG>(graphicsConfig.m_screenHeight)
        };
        commandList->RSSetScissorRects(1, &scissorRect);
        // プリミティブトポロジーの設定
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        // SwapChainのバックバッファをレンダーターゲットとして設定する
        UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
        DX12::ComPtr<ID3D12Resource> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(
            backBufferIndex,
            IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to get SwapChain back buffer.");
        }
        // バリア遷移(Present -> RenderTarget)
        {
            D3D12_RESOURCE_BARRIER barrierDesc{};
            barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrierDesc.Transition.pResource = backBuffer.Get();
            barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            commandList->ResourceBarrier(1, &barrierDesc);
        }
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
            m_descriptorAllocator.get_cpu_handle(m_swapChain.get_rtv_table(backBufferIndex));
        commandList->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
        // バックバッファのクリア
        commandList->ClearRenderTargetView(
            rtvHandle,
            graphicsConfig.m_clearColor.data(),
            0, nullptr);
        // バリア遷移(RenderTarget -> Present)
        {
            D3D12_RESOURCE_BARRIER barrierDesc{};
            barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrierDesc.Transition.pResource = backBuffer.Get();
            barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            commandList->ResourceBarrier(1, &barrierDesc);
        }
        // 5) コマンドリストの記録を終了する
        commandContext->close();
        // 6) コマンドリストの実行をキューに送る
        auto queueContext = m_renderDevice.get_queue_pool()->get_present_queue();
        queueContext->execute(commandContext);
        // 7) GPUの処理が完了するまで待機する
        queueContext->wait_fence();
        // 8) キューコンテキストをプールに返す
        // m_renderDevice.get_queue_pool()->return_queue(queueContext);
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
