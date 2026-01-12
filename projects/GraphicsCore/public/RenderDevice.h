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
        // DXGIファクトリ取得
        [[nodiscard]] IDXGIFactory7* get_dxgi_factory() const noexcept { return m_dxgiFactory.Get(); }
        // D3D12デバイス取得
        [[nodiscard]] ID3D12Device* get_d3d12_device() const noexcept { return m_d3d12Device.Get(); }
        [[nodiscard]] ID3D12Device1* get_d3d12_device1() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device1* device1 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device1));
            }
            return device1;
        }
        [[nodiscard]] ID3D12Device2* get_d3d12_device2() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device2* device2 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device2));
            }
            return device2;
        }
        [[nodiscard]] ID3D12Device3* get_d3d12_device3() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device3* device3 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device3));
            }
            return device3;
        }
        [[nodiscard]] ID3D12Device4* get_d3d12_device4() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device4* device4 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device4));
            }
            return device4;
        }
        [[nodiscard]] ID3D12Device5* get_d3d12_device5() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device5* device5 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device5));
            }
            return device5;
        }
        [[nodiscard]] ID3D12Device6* get_d3d12_device6() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device6* device6 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device6));
            }
            return device6;
        }
        [[nodiscard]] ID3D12Device7* get_d3d12_device7() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device7* device7 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device7));
            }
            return device7;
        }
        [[nodiscard]] ID3D12Device8* get_d3d12_device8() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device8* device8 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device8));
            }
            return device8;
        }
        [[nodiscard]] ID3D12Device9* get_d3d12_device9() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device9* device9 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device9));
            }
            return device9;
        }
        [[nodiscard]] ID3D12Device10* get_d3d12_device10() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device10* device10 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device10));
            }
            return device10;
        }
        [[nodiscard]] ID3D12Device11* get_d3d12_device11() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device11* device11 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device11));
            }
            return device11;
        }
        [[nodiscard]] ID3D12Device12* get_d3d12_device12() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device12* device12 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device12));
            }
            return device12;
        }
        [[nodiscard]] ID3D12Device13* get_d3d12_device13() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device13* device13 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device13));
            }
            return device13;
        }
        [[nodiscard]] ID3D12Device14* get_d3d12_device14() const noexcept
        {
            // 1) 利用可能なら特定バージョンのインターフェイスを取得する
            // 2) 取得できなければ nullptr を返す
            ID3D12Device14* device14 = nullptr;
            if (m_d3d12Device)
            {
                m_d3d12Device->QueryInterface(IID_PPV_ARGS(&device14));
            }
            return device14;
        }
    private:
        // DXGIファクトリ生成
        [[nodiscard]] Result create_dxgi_factory([[maybe_unused]] bool enableDebugLayer);
        // D3D12デバイス生成
        [[nodiscard]] Result create_d3d12_device();
        // D3D12オプション取得
        void query_d3d12_options() noexcept;
    private:
        ComPtr<IDXGIFactory7> m_dxgiFactory;// DXGIファクトリ
        ComPtr<ID3D12Device> m_d3d12Device; // D3D12デバイス
        DXGI_ADAPTER_DESC3 m_adapterDesc = {};// アダプタ情報
        D3D_FEATURE_LEVEL m_featureLevel = {};// 機能レベル

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

