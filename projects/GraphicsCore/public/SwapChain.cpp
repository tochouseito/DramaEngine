#include "pch.h"
#include "SwapChain.h"
#include "GraphicsConfig.h"
#include "RenderDevice.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics::DX12
{
    SwapChain::SwapChain(RenderDevice& device, DescriptorAllocator& descriptorAllocator, void* hWnd)
        : m_renderDevice(device)
        , m_descriptorAllocator(descriptorAllocator)
    {
        m_hWnd = reinterpret_cast<HWND>(hWnd);
    }

    // TableID取得
    DescriptorAllocator::TableID& SwapChain::get_rtv_table(uint32_t index)
    {
        return m_rtvTables[index];
    }
    Core::Error::Result SwapChain::create(uint32_t width, uint32_t height, uint32_t bufferCount)
    {
        m_desc.Width = width;// 画面の幅。ウィンドウのクライアント領域を同じものにしておく
        m_desc.Height = height;// 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
        m_desc.Format = graphicsConfig.m_ldrOffscreenFormat;// 色の形式
        m_desc.SampleDesc.Count = 1;// マルチサンプルしない
        m_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;// 描画のターゲットとして利用する
        m_desc.BufferCount = bufferCount;// バッファ数
        m_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;// モニタにうつしたら、中身を破棄
        m_desc.Flags =
            DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING |
            DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;// ティアリングサポート
        auto* queuePool = m_renderDevice.get_queue_pool();
        auto* graphicsQueue = queuePool->get_graphics_queue();
        if (!graphicsQueue)
        {
            Core::IO::LogAssert::assert_f(false, "Graphics queue is null.");
        }
        HRESULT hr = m_renderDevice.get_dxgi_factory()->CreateSwapChainForHwnd(
            graphicsQueue->get_command_queue(),
            m_hWnd,
            &m_desc,
            nullptr, nullptr,
            reinterpret_cast<IDXGISwapChain1**>(m_swapChain.GetAddressOf()));
        if (FAILED(hr))
        {
            Core::IO::LogAssert::assert_f(false, "Create SwapChain Failed!");
        }
        queuePool->return_queue(graphicsQueue);

        // 名前つけ
        SetDXGIName(m_swapChain.Get(), L"SwapChain");

        // リフレッシュレートを取得。floatで取るのは大変なので大体あってれば良いので整数で。
        // ウィンドウがあるモニターを取得
        HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfo(hMonitor, &mi))
        {
            Core::IO::LogAssert::assert_f(false, "Failed to get monitor info for refresh rate.");
        }
        // mi.szDeviceに対応するディスプレイ設定を取得
        DEVMODE dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
        {
            Core::IO::LogAssert::assert_f(false, "Failed to get display settings for refresh rate.");
        }
        m_refreshrate = static_cast<uint32_t>(dm.dmDisplayFrequency);
        graphicsConfig.m_displayRefreshrate = m_refreshrate;

        /*HDC hdc = GetDC(Platform::WinApp::m_HWND);
        m_SwapChainContext.m_RefreshRate = GetDeviceCaps(hdc, VREFRESH);
        ReleaseDC(Platform::WinApp::m_HWND, hdc);*/

        // VSync共存型FPS固定のためにレイテンシ1
        m_swapChain->SetMaximumFrameLatency(1);

        // OSが行うAlt+Enterのフルスクリーンは制御不能なので禁止
        m_renderDevice.get_dxgi_factory()->MakeWindowAssociation(
            m_hWnd,
            DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER);

        // バックバッファの取得とRTVの作成
        m_rtvTables.reserve(bufferCount);
        for (uint32_t i = 0; i < bufferCount; ++i)
        {
            ComPtr<ID3D12Resource> pResource;
            hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&pResource));
            if (FAILED(hr))
            {
                Core::IO::LogAssert::assert_f(false, "Get SwapChain back buffer Failed!");
            }
            DescriptorAllocator::TableID rtvTableID = m_descriptorAllocator.allocate(DescriptorAllocator::TableKind::RenderTargets);
            m_descriptorAllocator.create_rtv(rtvTableID, pResource.Get());
            m_rtvTables.push_back(rtvTableID);
        }

        return Core::Error::Result::ok();
    }
} // namespace Drama::Graphics::DX12
