#include "pch.h"
#include "RenderDevice.h"
#include <format>

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
        // D3D12デバイス生成
        result = create_d3d12_device();
        if (!result)
        {
            return result;
        }
        // D3D12オプション取得
        query_d3d12_options();
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
    Result RenderDevice::create_d3d12_device()
    {
        HRESULT hr;

        // 使用するアダプタ用の変数。最初にNullptrを入れておく
        Microsoft::WRL::ComPtr < IDXGIAdapter4> useAdapter = nullptr;

        // 良い順にアダプタを頼む
        for (UINT i = 0; m_dxgiFactory->EnumAdapterByGpuPreference(i,
            DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) !=
            DXGI_ERROR_NOT_FOUND; ++i)
        {

            // アダプターの情報を取得する
            hr = useAdapter->GetDesc3(&m_adapterDesc);

            if (FAILED(hr))
            {
                return Result::fail(
                    Core::Error::Facility::Graphics,
                    Core::Error::Code::GettingInfoFailed,
                    Core::Error::Severity::Error,
                    static_cast<uint32_t>(hr),
                    "Failed to get adapter description.");
            }

            // ソフトウェアアダプタでなければ
            if (!(m_adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
            {
                break;
            }
            // ソフトウェアアダプタの場合は見なかったことにする
            useAdapter = nullptr;
        }
        // 適切なアダプタが見つからなかったので起動できない
        if (useAdapter == nullptr)
        {
            Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::NotFound,
                Core::Error::Severity::Error,
                0,
                "Failed to find a suitable GPU adapter.");
        }

        // 機能レベルとログ出力用の文字列
        D3D_FEATURE_LEVEL featureLevels[] = {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
        };
        const char* featureLevelStrings[] = {
            "12.2",
            "12.1",
            "12.0"
        };

        // 高い順に生成できるか試していく
        for (size_t i = 0; i < _countof(featureLevels); ++i)
        {

            // 採用したアダプターでデバイスを生成
            hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&m_d3d12Device));

            // 指定した機能レベルでデバイスが生成できたか確認
            if (SUCCEEDED(hr))
            {
                // 生成できたのでループを抜ける
                m_featureLevel = featureLevels[i];
                break;
            }
        }
        // デバイスの生成がうまくいかなかったので起動できない
        if (m_d3d12Device == nullptr)
        {
            return Result::fail(
                Core::Error::Facility::Graphics,
                Core::Error::Code::CreationFailed,
                Core::Error::Severity::Error,
                static_cast<uint32_t>(hr),
                "Failed to create D3D12 Device.");
        }

        // 名前つけ
        SetD3D12Name(m_d3d12Device.Get(), L"RenderDevice_D3D12Device");
#ifndef NDEBUG
        ComPtr<ID3D12InfoQueue> infoQueue;
        // フィルタリングを一時的に無効化してみる

        if (SUCCEEDED(m_d3d12Device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        {
            // ヤバいエラー時に止まる
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);

            // エラー時に止まる
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

            // 警告時に止まる
            infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

            infoQueue->SetBreakOnID(D3D12_MESSAGE_ID_FENCE_ZERO_WAIT, TRUE);

            // 抑制するメッセージのID
            D3D12_MESSAGE_ID denyIds[] = {

                // Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
                // https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
                D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
                D3D12_MESSAGE_ID_GPU_BASED_VALIDATION_RESOURCE_STATE_IMPRECISE,  // = 1044 相当
                D3D12_MESSAGE_ID_FENCE_ZERO_WAIT // ImGuiが原因で出る。無視してよい
            };

            // 抑制するレベル
            D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
            D3D12_INFO_QUEUE_FILTER filter{};
            filter.DenyList.NumIDs = _countof(denyIds);
            filter.DenyList.pIDList = denyIds;
            filter.DenyList.NumSeverities = _countof(severities);
            filter.DenyList.pSeverityList = severities;

            // 指定したメッセージの表示を抑制する
            infoQueue->PushStorageFilter(&filter);
        }
#endif // DEBUG
        return Result::ok();
    }
    void RenderDevice::query_d3d12_options() noexcept
    {

    }
}
