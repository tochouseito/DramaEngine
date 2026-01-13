#include "pch.h"
#include "GpuCommand.h"
#include "RenderDevice.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics::DX12
{
    CommandContext::CommandContext(RenderDevice& renderDevice, D3D12_COMMAND_LIST_TYPE type)
        : m_renderDevice(renderDevice)
    {
        ID3D12Device* device = m_renderDevice.get_d3d12_device();
        // 1) コマンドアロケータの作成
        HRESULT hr = device->CreateCommandAllocator(
            type,
            IID_PPV_ARGS(&m_commandAllocator));
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to create CommandAllocator.");
        }
        SetD3D12Name(m_commandAllocator.Get(), L"CommandContext CommandAllocator");
        // 2) コマンドリストの作成
        hr = device->CreateCommandList(
            0,
            type,
            m_commandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&m_commandList));
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to create GraphicsCommandList.");
        }
        SetD3D12Name(m_commandList.Get(), L"CommandContext CommandList");

        // 3) コマンドリストは生成直後にオープン状態になるのでクローズしておく
        m_commandList->Close();
    }
    void CommandContext::reset()
    {
        // 1) コマンドアロケータのリセット
        HRESULT hr = m_commandAllocator->Reset();
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to reset CommandAllocator.");
        }
        // 2) コマンドリストのリセット
        hr = m_commandList->Reset(
            m_commandAllocator.Get(),
            nullptr);
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to reset GraphicsCommandList.");
        }
    }
    void CommandContext::close()
    {
        // コマンドリストのクローズ
        HRESULT hr = m_commandList->Close();
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to close GraphicsCommandList.");
        }
    }
    QueueContext::QueueContext(RenderDevice& device, D3D12_COMMAND_LIST_TYPE type)
    {
        // フェンスの作成
        m_fence.Reset();
        m_fenceValue = 0;// 初期値0
        HRESULT hr = device.get_d3d12_device()->CreateFence(
            m_fenceValue,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&m_fence));
        if (FAILED(hr))
            {
            Core::IO::LogAssert::assert(false, "Failed to create Fence.");
        }
        SetD3D12Name(m_fence.Get(), L"QueueContext Fence");
        m_fenceValue++;// 次回以降のシグナル用にインクリメントしておく
        // イベントハンドルの作成
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            Core::IO::LogAssert::assert(false, "Failed to create Fence event.");
        }
        // コマンドキューの作成
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = type;
        hr = device.get_d3d12_device()->CreateCommandQueue(
            &queueDesc,
            IID_PPV_ARGS(&m_commandQueue));
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert(false, "Failed to create CommandQueue.");
        }
        SetD3D12Name(m_commandQueue.Get(), L"QueueContext CommandQueue");
    }
} // namespace Drama::Graphics::DX12
