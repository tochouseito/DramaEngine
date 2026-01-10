#pragma once
#include "stdafx.h"
#include <dxgi1_6.h>

namespace Drama::Graphics::DX12
{
    class RenderDevice
    {
        public:
        RenderDevice() = default;
        ~RenderDevice() = default;
        // 初期化
        [[nodiscard]] Result initialize(bool enableDebugLayer = false);
    private:
        // DXGIファクトリ生成
        [[nodiscard]] Result create_dxgi_factory([[maybe_unused]] bool enableDebugLayer);
        // D3D12デバイス生成
        [[nodiscard]] Result create_d3d12_device();
    private:
        ComPtr<IDXGIFactory7> m_dxgiFactory;// DXGIファクトリ
        ComPtr<ID3D12Device> m_d3d12Device; // D3D12デバイス
        DXGI_ADAPTER_DESC3 m_adapterDesc = {};// アダプタ情報
        D3D_FEATURE_LEVEL m_featureLevel;// 機能レベル

        /*==================== D3D12Options ====================*/
        D3D12_FEATURE_DATA_D3D12_OPTIONS m_options = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 m_options1 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS2 m_options2 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS3 m_options3 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS4 m_options4 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 m_options5 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 m_options6 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 m_options7 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS8 m_options8 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS9 m_options9 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS10 m_options10 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS11 m_options11 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS12 m_options12 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS13 m_options13 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS14 m_options14 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS15 m_options15 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS16 m_options16 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS17 m_options17 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS18 m_options18 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS19 m_options19 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS20 m_options20 = {};
        D3D12_FEATURE_DATA_D3D12_OPTIONS21 m_options21 = {};
    };
}

