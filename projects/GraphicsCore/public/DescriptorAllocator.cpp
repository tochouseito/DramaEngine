#include "pch.h"
#include "DescriptorAllocator.h"

// Drama includes
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics::DX12
{
    Core::Error::Result DescriptorAllocator::Initialize(uint32_t texCap, uint32_t bufCap, uint32_t rtCap, uint32_t dsCap)
    {
        HRESULT hr = S_OK;
        ID3D12Device* device = m_renderDevice.get_d3d12_device();
        if (!device)
        {
            Core::IO::LogAssert::assert(false, "DescriptorAllocator", "RenderDevice is not initialized.");
        }
        for (size_t i = 0; i < static_cast<size_t>(HeapType::kCount); ++i)
        {
            D3D12_DESCRIPTOR_HEAP_TYPE heapType{};
            D3D12_DESCRIPTOR_HEAP_DESC desc{};
            switch (static_cast<HeapType>(i))
            {
            case HeapType::CBV_SRV_UAV:
            {
                // CPU 可視ヒープ
                heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                m_DescriptorSizes[i] = device->GetDescriptorHandleIncrementSize(heapType);

                desc.Type = heapType;
                desc.NumDescriptors = texCap + bufCap;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeaps[i]));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_DescriptorHeaps[i].Get(), L"DescriptorHeap");

                // GPU 可視ヒープ
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_GpuSUVHeap));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_GpuSUVHeap.Get(), L"GpuCBV_SRV_UAV_Heap");

                // テーブル初期化（Textures / Buffers）
                m_Textures.heapType = HeapType::CBV_SRV_UAV;
                m_Buffers.heapType = HeapType::CBV_SRV_UAV;

                m_Textures.capacity = texCap;
                m_Textures.baseIndex = 0;
                m_Textures.freeList.reserve(m_Textures.capacity);
                for (uint32_t j = 0; j < texCap; ++j)
                {
                    m_Textures.freeList.push_back(m_Textures.capacity - 1u - j);
                }

                m_Buffers.capacity = bufCap;
                m_Buffers.baseIndex = texCap;
                m_Buffers.freeList.reserve(m_Buffers.capacity);
                for (uint32_t j = 0; j < bufCap; ++j)
                {
                    m_Buffers.freeList.push_back(m_Buffers.capacity - 1u - j);
                }
            }
            break;

            case HeapType::SAMPLER:
                // サンプラーヒープは未実装
                heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
                break;

            case HeapType::RTV:
            {
                heapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
                m_DescriptorSizes[i] = device->GetDescriptorHandleIncrementSize(heapType);

                desc.Type = heapType;
                desc.NumDescriptors = rtCap;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeaps[i]));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_DescriptorHeaps[i].Get(), L"DescriptorHeap");

                m_RenderTargets.heapType = HeapType::RTV;
                m_RenderTargets.capacity = rtCap;
                m_RenderTargets.freeList.reserve(m_RenderTargets.capacity);
                for (uint32_t j = 0; j < rtCap; ++j)
                {
                    m_RenderTargets.freeList.push_back(m_RenderTargets.capacity - 1u - j);
                }
            }
            break;

            case HeapType::DSV:
            {
                heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                m_DescriptorSizes[i] = device->GetDescriptorHandleIncrementSize(heapType);

                desc.Type = heapType;
                desc.NumDescriptors = dsCap;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_DescriptorHeaps[i]));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_DescriptorHeaps[i].Get(), L"DescriptorHeap");

                m_DepthStencils.heapType = HeapType::DSV;
                m_DepthStencils.capacity = dsCap;
                m_DepthStencils.freeList.reserve(m_DepthStencils.capacity);
                for (uint32_t j = 0; j < dsCap; ++j)
                {
                    m_DepthStencils.freeList.push_back(m_DepthStencils.capacity - 1u - j);
                }
            }
            break;

            default:
                Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Unknown HeapType");
                break;
            }
        }
        return Core::Error::Result::ok();
    }
    DescriptorAllocator::TableID DescriptorAllocator::Allocate(TableKind k)
    {
        Table& t = GetTable(k);
        if (t.freeList.empty())
        {
            Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Descriptor table full, need to expand.");
            EnsureCapacity(k, /*needOneMore=*/1);
        }

        if (t.freeList.empty())
        {
            // EnsureCapacity が未実装なので、ここに来たら致命的
            return TableID{ k, t.generation, TableID::Invalid };
        }

        uint32_t idx = t.freeList.back();
        t.freeList.pop_back();
        return TableID{ k, t.generation, idx };
    }
    void DescriptorAllocator::Free(const TableID& id)
    {
        if (!id.valid())
        {
            return;
        }

        Table& t = GetTable(id.kind);
        t.freeList.push_back(id.index);
        // 任意：ヌル SRV/UAV を書いておくと安全
    }
    void DescriptorAllocator::CreateCBV(TableID& id, GpuBuffer* buf)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
        desc.BufferLocation = buf->GetResource()->GetGPUVirtualAddress();
        desc.SizeInBytes = (static_cast<UINT>(buf->GetBufferSize()) + 255u) & ~255u; // 256 バイトアライメント

        auto cpuH = GetCPUHandle(id);
        m_renderDevice.get_d3d12_device()->CreateConstantBufferView(&desc, cpuH);

        // CBV はシェーダから参照するので GPU ヒープにもコピー
        CopyToGpuHeap(id);
    }
    void DescriptorAllocator::CreateSRVBuffer(TableID& id, GpuBuffer* buf)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        desc.Buffer.NumElements = buf->GetNumElements();
        desc.Buffer.StructureByteStride = buf->GetStructureByteStride();

        auto cpuH = GetCPUHandle(id);
        m_renderDevice.get_d3d12_device()->CreateShaderResourceView(buf->GetResource(), &desc, cpuH);

        // GPU ヒープにもコピー
        CopyToGpuHeap(id);
    }
    void DescriptorAllocator::CreateUAVBuffer(TableID& id, GpuBuffer* buf)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        desc.Buffer.NumElements = buf->GetNumElements();
        desc.Buffer.StructureByteStride = buf->GetStructureByteStride();
        desc.Buffer.CounterOffsetInBytes = 0;

        auto cpuH = GetCPUHandle(id);
        m_renderDevice.get_d3d12_device()->CreateUnorderedAccessView(buf->GetResource(), nullptr, &desc, cpuH);

        // UAV も GPU ヒープへコピー
        CopyToGpuHeap(id);
    }
    void DescriptorAllocator::CreateUAVRawBuffer(TableID& id, GpuBuffer* buf)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_TYPELESS; // RAW のお作法
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        desc.Buffer.NumElements = buf->GetNumElements();
        desc.Buffer.StructureByteStride = 0;
        desc.Buffer.CounterOffsetInBytes = 0;

        auto cpuH = GetCPUHandle(id);
        m_renderDevice.get_d3d12_device()->CreateUnorderedAccessView(buf->GetResource(), nullptr, &desc, cpuH);

        // Raw UAV も GPU ヒープへコピー
        CopyToGpuHeap(id);
    }
    void DescriptorAllocator::CreateSRVTexture2D(TableID& id, GpuResource* res)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = g_GraphicsConfig.ldrOffscreenFormat;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;

        auto cpuH = GetCPUHandle(id);
        m_renderDevice.get_d3d12_device()->CreateShaderResourceView(res->GetResource(), &desc, cpuH);

        // SRV も GPU ヒープへミラーを作成
        CopyToGpuHeap(id);
    }
    void DescriptorAllocator::CreateRTV(TableID& id, GpuResource* res)
    {
        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = g_GraphicsConfig.ldrOffscreenFormat;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2D テクスチャとして書き込む

        m_renderDevice.get_d3d12_device()->CreateRenderTargetView(res->GetResource(), &desc, GetCPUHandle(id));
    }
    void DescriptorAllocator::CreateDSV(TableID& id, GpuResource* res)
    {
        // DSVの設定
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = g_GraphicsConfig.ldrOffscreenFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        m_renderDevice.get_d3d12_device()->CreateDepthStencilView(res->GetResource(), &dsvDesc, GetCPUHandle(id));
    }
    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetTableBaseGPU(TableKind k)
    {
        Table& t = GetTable(k);

        D3D12_GPU_DESCRIPTOR_HANDLE h{};

        if (t.heapType == HeapType::CBV_SRV_UAV)
        {
            // CBV_SRV_UAV は GPU 可視ヒープから取得
            h = m_GpuSUVHeap->GetGPUDescriptorHandleForHeapStart();
        }
        else
        {
            h = m_DescriptorHeaps[static_cast<size_t>(t.heapType)]->GetGPUDescriptorHandleForHeapStart();
        }

        h.ptr += static_cast<SIZE_T>(m_DescriptorSizes[static_cast<size_t>(t.heapType)]) * (t.baseIndex);
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetGPUHandle(TableID id)
    {
        Table& t = GetTable(id.kind);

        D3D12_GPU_DESCRIPTOR_HANDLE h{};

        if (t.heapType == HeapType::CBV_SRV_UAV)
        {
            // CBV_SRV_UAV は GPU 可視ヒープから取得
            h = m_GpuSUVHeap->GetGPUDescriptorHandleForHeapStart();
        }
        else
        {
            h = m_DescriptorHeaps[static_cast<size_t>(t.heapType)]->GetGPUDescriptorHandleForHeapStart();
        }

        h.ptr += static_cast<SIZE_T>(m_DescriptorSizes[static_cast<size_t>(t.heapType)]) * (t.baseIndex + id.index);
        return h;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(TableID id)
    {
        Table& t = GetTable(id.kind);
        D3D12_CPU_DESCRIPTOR_HANDLE h =
            m_DescriptorHeaps[static_cast<size_t>(t.heapType)]->GetCPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(m_DescriptorSizes[static_cast<size_t>(t.heapType)]) * (t.baseIndex + id.index);
        return h;
    }
    void DescriptorAllocator::EnsureCapacity(TableKind k, uint32_t needOneMore)
    {
        needOneMore;

        // 今は何もしない。将来的にヒープ再構築時に使用する。
        switch (k)
        {
        case TableKind::Textures:
        {
            /*uint32_t newTex = m_Textures.capacity;
            uint32_t newBuf = m_Buffers.capacity;
            newTex = std::max(1u, newTex * 2);
            RecreateHeap(TableKind::Textures, newTex, newBuf);*/
        }
        break;

        case TableKind::Buffers:
        {
            /*uint32_t newTex = m_Textures.capacity;
            uint32_t newBuf = m_Buffers.capacity;
            newBuf = std::max(1u, newBuf * 2);
            RecreateHeap(TableKind::Buffers, newTex, newBuf);*/
        }
        break;

        case TableKind::RenderTargets:
        {
            /*uint32_t newRT = m_RenderTargets.capacity;
            newRT = std::max(1u, newRT * 2);
            RecreateHeap(TableKind::RenderTargets, newRT, 0);*/
        }
        break;

        case TableKind::DepthStencils:
        {
            /*uint32_t newDS = m_DepthStencils.capacity;
            newDS = std::max(1u, newDS * 2);
            RecreateHeap(TableKind::DepthStencils, newDS, 0);*/
        }
        break;

        default:
            break;
        }
    }
    void DescriptorAllocator::RecreateHeap(TableKind k, uint32_t newCap, uint32_t newBufCap)
    {
        // TODO: ヒープ再構築（GPU フェンスと連携して RetiredHeap を使う）
        k; newCap; newBufCap;

        //// 旧ヒープを退役リストに（フェンスで寿命管理するのが理想）
        //if (m_heap)
        //{
        //    m_retired.push_back({ m_heap, /*fenceValue*/ 0 }); // ★フェンス値を外から渡す設計推奨
        //}

        //// 新ヒープ作成
        //D3D12_DESCRIPTOR_HEAP_DESC desc{};
        //desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        //desc.NumDescriptors = newCap + newBufCap;
        //desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        //Core::LogAssert::Verify(
        //    m_pRenderDevice->m_Device->CreateDescriptorHeap(
        //        &desc, IID_PPV_ARGS(&m_heap)), "DescriptorAllocator", "Failed CreateDescriptorHeap");

        //// baseIndex / capacity / generation を更新
        //m_Textures.baseIndex = texBase;
        //m_Textures.capacity  = newCap;
        //m_Textures.generation++;

        //m_Buffers.baseIndex = bufBase;
        //m_Buffers.capacity  = newBufCap;
        //m_Buffers.generation++;
    }
    DescriptorAllocator::Table& DescriptorAllocator::GetTable(TableKind k)
    {
        switch (k)
        {
        case TableKind::Textures:
            return m_Textures;
        case TableKind::Buffers:
            return m_Buffers;
        case TableKind::RenderTargets:
            return m_RenderTargets;
        case TableKind::DepthStencils:
            return m_DepthStencils;
        default:
            Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Unknown TableKind");
            break;
        }

        // ここには来ないはずだが、コンパイラを黙らせるために返しておく
        return m_Textures;
    }
    void DescriptorAllocator::CopyToGpuHeap(const TableID& id)
    {
        Table& t = GetTable(id.kind);

        // CBV_SRV_UAV 以外は GPU ヒープを持たない
        if (t.heapType != HeapType::CBV_SRV_UAV)
        {
            return;
        }

        Core::IO::LogAssert::assert(m_GpuSUVHeap != nullptr,
            "DescriptorAllocator", "GPU CBV_SRV_UAV heap is null");

        // CPU ヒープ上の元ディスクリプタ
        D3D12_CPU_DESCRIPTOR_HANDLE src = GetCPUHandle(id);

        // GPU 可視ヒープ側の同じスロット位置にコピー
        D3D12_CPU_DESCRIPTOR_HANDLE dst = m_GpuSUVHeap->GetCPUDescriptorHandleForHeapStart();

        const SIZE_T inc =
            static_cast<SIZE_T>(m_DescriptorSizes[static_cast<size_t>(HeapType::CBV_SRV_UAV)]);
        const uint32_t slotIndex = t.baseIndex + id.index;

        dst.ptr += inc * slotIndex;

        m_renderDevice.get_d3d12_device()->CopyDescriptorsSimple(
            1,
            dst, // dest (GPU ヒープ)
            src, // src  (CPU ヒープ)
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
} // namespace Drama::Graphics::DX12
