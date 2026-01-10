#include "pch.h"
#include "RenderDevice.h"

namespace Drama::Graphics::DX12
{
    Result RenderDevice::initialize(bool enableDebugLayer)
    {
        Result result;
        // DXGIファクトリ生成
        result = create_dxgi_factory(enableDebugLayer);
        if (!result)
        {
            return result;
        }
        return Result::ok();
    }
    Result RenderDevice::create_dxgi_factory([[maybe_unused]] bool enableDebugLayer)
    {
#ifndef NDEBUG
        /*
        [ INITIALIZATION MESSAGE #1016: CREATEDEVICE_DEBUG_LAYER_STARTUP_OPTIONS]
        Debug時のみの警告なため無視
        */
        ComPtr<ID3D12Debug6> debugController;
        if (enableDebugLayer)
        {
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            {
                // デバッグレイヤーを有効化する
                debugController->EnableDebugLayer();

                // さらにGPU側でもチェックを行うようにする
                debugController->SetEnableGPUBasedValidation(TRUE);
            }
        }
        ComPtr<ID3D12DeviceRemovedExtendedDataSettings> deviceRemoved;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&deviceRemoved))))
        {
            deviceRemoved->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
            deviceRemoved->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
        }
#endif
        HRESULT hr;
        hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_dxgiFactory));
        if (FAILED(hr))
        {
            return Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::CreationFailed,
                Core::Error::Severity::Error,
                static_cast<uint32_t>(hr),
                "Failed to create DXGI Factory.");
        }
        SetDXGIName(m_dxgiFactory.Get(), L"RenderDevice_DXGIFactory");
        return Result::ok();
    }
}
