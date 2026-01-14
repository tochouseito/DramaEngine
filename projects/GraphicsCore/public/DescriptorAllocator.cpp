#include "pch.h"
#include "DescriptorAllocator.h"

// Drama includes
#include "GraphicsCore/public/RenderDevice.h"
#include "GraphicsCore/public/GraphicsConfig.h"
#include "Core/IO/public/LogAssert.h"

namespace Drama::Graphics::DX12
{
    Core::Error::Result DescriptorAllocator::initialize(uint32_t texCap, uint32_t bufCap, uint32_t rtCap, uint32_t dsCap)
    {
        // 1) デバイスを取得してヒープ作成の前提を整える
        // 2) ヒープ種別ごとにディスクリプタヒープとテーブルを初期化する
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
                m_descriptorSizes[i] = device->GetDescriptorHandleIncrementSize(heapType);

                desc.Type = heapType;
                desc.NumDescriptors = texCap + bufCap;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeaps[i]));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_descriptorHeaps[i].Get(), L"DescriptorHeap");

                // GPU 可視ヒープ
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_gpuSrvUavHeap));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_gpuSrvUavHeap.Get(), L"GpuCBV_SRV_UAV_Heap");

                // テーブル初期化（Textures / Buffers）
                m_textures.m_heapType = HeapType::CBV_SRV_UAV;
                m_buffers.m_heapType = HeapType::CBV_SRV_UAV;

                m_textures.m_capacity = texCap;
                m_textures.m_baseIndex = 0;
                m_textures.m_freeList.reserve(m_textures.m_capacity);
                for (uint32_t j = 0; j < texCap; ++j)
                {
                    m_textures.m_freeList.push_back(m_textures.m_capacity - 1u - j);
                }

                m_buffers.m_capacity = bufCap;
                m_buffers.m_baseIndex = texCap;
                m_buffers.m_freeList.reserve(m_buffers.m_capacity);
                for (uint32_t j = 0; j < bufCap; ++j)
                {
                    m_buffers.m_freeList.push_back(m_buffers.m_capacity - 1u - j);
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
                m_descriptorSizes[i] = device->GetDescriptorHandleIncrementSize(heapType);

                desc.Type = heapType;
                desc.NumDescriptors = rtCap;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeaps[i]));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_descriptorHeaps[i].Get(), L"DescriptorHeap");

                m_renderTargets.m_heapType = HeapType::RTV;
                m_renderTargets.m_capacity = rtCap;
                m_renderTargets.m_freeList.reserve(m_renderTargets.m_capacity);
                for (uint32_t j = 0; j < rtCap; ++j)
                {
                    m_renderTargets.m_freeList.push_back(m_renderTargets.m_capacity - 1u - j);
                }
            }
            break;

            case HeapType::DSV:
            {
                heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
                m_descriptorSizes[i] = device->GetDescriptorHandleIncrementSize(heapType);

                desc.Type = heapType;
                desc.NumDescriptors = dsCap;
                desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
                hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_descriptorHeaps[i]));
                if (FAILED(hr))
                {
                    Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Failed CreateDescriptorHeap");
                }
                SetD3D12Name(m_descriptorHeaps[i].Get(), L"DescriptorHeap");

                m_depthStencils.m_heapType = HeapType::DSV;
                m_depthStencils.m_capacity = dsCap;
                m_depthStencils.m_freeList.reserve(m_depthStencils.m_capacity);
                for (uint32_t j = 0; j < dsCap; ++j)
                {
                    m_depthStencils.m_freeList.push_back(m_depthStencils.m_capacity - 1u - j);
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
    DescriptorAllocator::TableID DescriptorAllocator::allocate(TableKind k)
    {
        // 1) 対象テーブルを取得して空き状況を確認する
        // 2) 空きがあれば割り当て済みインデックスを返す
        Table& t = get_table(k);
        if (t.m_freeList.empty())
        {
            Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Descriptor table full, need to expand.");
            ensure_capacity(k, /*needOneMore=*/1);
        }

        if (t.m_freeList.empty())
        {
            // ensure_capacity が未実装なので、ここに来たら致命的
            return TableID{ k, t.m_generation, TableID::kInvalid };
        }

        uint32_t idx = t.m_freeList.back();
        t.m_freeList.pop_back();
        return TableID{ k, t.m_generation, idx };
    }
    void DescriptorAllocator::free_table(const TableID& id)
    {
        // 1) 無効 ID を弾いて再利用ミスを防ぐ
        // 2) 空きリストに戻して再割り当て可能にする
        if (!id.valid())
        {
            return;
        }

        Table& t = get_table(id.m_kind);
        t.m_freeList.push_back(id.m_index);
        // 任意：ヌル SRV/UAV を書いておくと安全
    }
    void DescriptorAllocator::create_cbv(TableID& id, GpuBuffer* buf)
    {
        // 1) 定数バッファビューの記述子を構築する
        // 2) CPU ヒープへ登録し GPU ヒープへ反映する
        D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
        desc.BufferLocation = buf->get_resource()->GetGPUVirtualAddress();
        desc.SizeInBytes = (static_cast<UINT>(buf->get_buffer_size()) + 255u) & ~255u; // 256 バイトアライメント

        auto cpuH = get_cpu_handle(id);
        m_renderDevice.get_d3d12_device()->CreateConstantBufferView(&desc, cpuH);

        // CBV はシェーダから参照するので GPU ヒープにもコピー
        copy_to_gpu_heap(id);
    }
    void DescriptorAllocator::create_srv_buffer(TableID& id, GpuBuffer* buf)
    {
        // 1) バッファ用の SRV 記述子を組み立てる
        // 2) CPU/GPU 両ヒープへ反映する
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        desc.Buffer.NumElements = buf->get_num_elements();
        desc.Buffer.StructureByteStride = buf->get_structure_byte_stride();

        auto cpuH = get_cpu_handle(id);
        m_renderDevice.get_d3d12_device()->CreateShaderResourceView(buf->get_resource(), &desc, cpuH);

        // GPU ヒープにもコピー
        copy_to_gpu_heap(id);
    }
    void DescriptorAllocator::create_uav_buffer(TableID& id, GpuBuffer* buf)
    {
        // 1) UAV 記述子を組み立てる
        // 2) CPU/GPU 両ヒープへ反映する
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        desc.Buffer.NumElements = buf->get_num_elements();
        desc.Buffer.StructureByteStride = buf->get_structure_byte_stride();
        desc.Buffer.CounterOffsetInBytes = 0;

        auto cpuH = get_cpu_handle(id);
        m_renderDevice.get_d3d12_device()->CreateUnorderedAccessView(buf->get_resource(), nullptr, &desc, cpuH);

        // UAV も GPU ヒープへコピー
        copy_to_gpu_heap(id);
    }
    void DescriptorAllocator::create_uav_raw_buffer(TableID& id, GpuBuffer* buf)
    {
        // 1) RAW UAV の記述子を組み立てる
        // 2) CPU/GPU 両ヒープへ反映する
        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R32_TYPELESS; // RAW のお作法
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        desc.Buffer.NumElements = buf->get_num_elements();
        desc.Buffer.StructureByteStride = 0;
        desc.Buffer.CounterOffsetInBytes = 0;

        auto cpuH = get_cpu_handle(id);
        m_renderDevice.get_d3d12_device()->CreateUnorderedAccessView(buf->get_resource(), nullptr, &desc, cpuH);

        // Raw UAV も GPU ヒープへコピー
        copy_to_gpu_heap(id);
    }
    void DescriptorAllocator::create_srv_texture_2d(TableID& id, ID3D12Resource* res)
    {
        // 1) テクスチャ用 SRV の記述子を組み立てる
        // 2) CPU/GPU 両ヒープへ反映する
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = graphicsConfig.m_ldrOffscreenFormat;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;

        auto cpuH = get_cpu_handle(id);
        m_renderDevice.get_d3d12_device()->CreateShaderResourceView(res, &desc, cpuH);

        // SRV も GPU ヒープへミラーを作成
        copy_to_gpu_heap(id);
    }
    void DescriptorAllocator::create_rtv(TableID& id, ID3D12Resource* res)
    {
        // 1) RTV 記述子を準備する
        // 2) CPU ヒープへ反映する
        D3D12_RENDER_TARGET_VIEW_DESC desc = {};
        desc.Format = graphicsConfig.m_ldrOffscreenFormat;
        desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2D テクスチャとして書き込む

        m_renderDevice.get_d3d12_device()->CreateRenderTargetView(res, &desc, get_cpu_handle(id));
    }
    void DescriptorAllocator::create_dsv(TableID& id, ID3D12Resource* res)
    {
        // 1) DSV 記述子を準備する
        // 2) CPU ヒープへ反映する
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = graphicsConfig.m_ldrOffscreenFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        m_renderDevice.get_d3d12_device()->CreateDepthStencilView(res, &dsvDesc, get_cpu_handle(id));
    }
    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::get_table_base_gpu(TableKind k)
    {
        // 1) テーブル情報を取得する
        // 2) ヒープ種別に応じて GPU ハンドルを返す
        Table& t = get_table(k);

        D3D12_GPU_DESCRIPTOR_HANDLE h{};

        if (t.m_heapType == HeapType::CBV_SRV_UAV)
        {
            // CBV_SRV_UAV は GPU 可視ヒープから取得
            h = m_gpuSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        }
        else
        {
            h = m_descriptorHeaps[static_cast<size_t>(t.m_heapType)]->GetGPUDescriptorHandleForHeapStart();
        }

        h.ptr += static_cast<SIZE_T>(m_descriptorSizes[static_cast<size_t>(t.m_heapType)]) * (t.m_baseIndex);
        return h;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::get_gpu_handle(TableID id)
    {
        // 1) テーブル情報を取得してベースハンドルを決める
        // 2) インデックス分だけオフセットして返す
        Table& t = get_table(id.m_kind);

        D3D12_GPU_DESCRIPTOR_HANDLE h{};

        if (t.m_heapType == HeapType::CBV_SRV_UAV)
        {
            // CBV_SRV_UAV は GPU 可視ヒープから取得
            h = m_gpuSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        }
        else
        {
            h = m_descriptorHeaps[static_cast<size_t>(t.m_heapType)]->GetGPUDescriptorHandleForHeapStart();
        }

        h.ptr += static_cast<SIZE_T>(m_descriptorSizes[static_cast<size_t>(t.m_heapType)]) * (t.m_baseIndex + id.m_index);
        return h;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::get_cpu_handle(TableID id)
    {
        // 1) テーブル情報を取得してベースハンドルを決める
        // 2) インデックス分だけオフセットして返す
        Table& t = get_table(id.m_kind);
        D3D12_CPU_DESCRIPTOR_HANDLE h =
            m_descriptorHeaps[static_cast<size_t>(t.m_heapType)]->GetCPUDescriptorHandleForHeapStart();
        h.ptr += static_cast<SIZE_T>(m_descriptorSizes[static_cast<size_t>(t.m_heapType)]) * (t.m_baseIndex + id.m_index);
        return h;
    }
    void DescriptorAllocator::ensure_capacity(TableKind k, uint32_t needOneMore)
    {
        // 1) 将来の拡張フックとして引数を受けておく
        // 2) 現状は無処理で戻る
        needOneMore;

        // 今は何もしない。将来的にヒープ再構築時に使用する。
        switch (k)
        {
        case TableKind::Textures:
        {
            /*uint32_t newTex = m_textures.m_capacity;
            uint32_t newBuf = m_buffers.m_capacity;
            newTex = std::max(1u, newTex * 2);
            recreate_heap(TableKind::Textures, newTex, newBuf);*/
        }
        break;

        case TableKind::Buffers:
        {
            /*uint32_t newTex = m_textures.m_capacity;
            uint32_t newBuf = m_buffers.m_capacity;
            newBuf = std::max(1u, newBuf * 2);
            recreate_heap(TableKind::Buffers, newTex, newBuf);*/
        }
        break;

        case TableKind::RenderTargets:
        {
            /*uint32_t newRT = m_renderTargets.m_capacity;
            newRT = std::max(1u, newRT * 2);
            recreate_heap(TableKind::RenderTargets, newRT, 0);*/
        }
        break;

        case TableKind::DepthStencils:
        {
            /*uint32_t newDS = m_depthStencils.m_capacity;
            newDS = std::max(1u, newDS * 2);
            recreate_heap(TableKind::DepthStencils, newDS, 0);*/
        }
        break;

        default:
            break;
        }
    }
    void DescriptorAllocator::recreate_heap(TableKind k, uint32_t newCap, uint32_t newBufCap)
    {
        // 1) 再構築の予定だけを残し、現時点では無処理にする
        // 2) 引数は将来の実装に備えて保持する
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
        //m_textures.m_baseIndex = texBase;
        //m_textures.m_capacity  = newCap;
        //m_textures.m_generation++;

        //m_buffers.m_baseIndex = bufBase;
        //m_buffers.m_capacity  = newBufCap;
        //m_buffers.m_generation++;
    }
    DescriptorAllocator::Table& DescriptorAllocator::get_table(TableKind k)
    {
        // 1) 種別に対応するテーブルを返す
        // 2) 不正値はアサートして最後に既定値を返す
        switch (k)
        {
        case TableKind::Textures:
            return m_textures;
        case TableKind::Buffers:
            return m_buffers;
        case TableKind::RenderTargets:
            return m_renderTargets;
        case TableKind::DepthStencils:
            return m_depthStencils;
        default:
            Core::IO::LogAssert::assert(false, "DescriptorAllocator", "Unknown TableKind");
            break;
        }

        // ここには来ないはずだが、コンパイラを黙らせるために返しておく
        return m_textures;
    }
    void DescriptorAllocator::copy_to_gpu_heap(const TableID& id)
    {
        // 1) 対象テーブルとヒープ種別を確認する
        // 2) CPU ヒープの記述子を GPU 可視ヒープへコピーする
        Table& t = get_table(id.m_kind);

        // CBV_SRV_UAV 以外は GPU ヒープを持たない
        if (t.m_heapType != HeapType::CBV_SRV_UAV)
        {
            return;
        }

        Core::IO::LogAssert::assert(m_gpuSrvUavHeap != nullptr,
            "DescriptorAllocator", "GPU CBV_SRV_UAV heap is null");

        // CPU ヒープ上の元ディスクリプタ
        D3D12_CPU_DESCRIPTOR_HANDLE src = get_cpu_handle(id);

        // GPU 可視ヒープ側の同じスロット位置にコピー
        D3D12_CPU_DESCRIPTOR_HANDLE dst = m_gpuSrvUavHeap->GetCPUDescriptorHandleForHeapStart();

        const SIZE_T inc =
            static_cast<SIZE_T>(m_descriptorSizes[static_cast<size_t>(HeapType::CBV_SRV_UAV)]);
        const uint32_t slotIndex = t.m_baseIndex + id.m_index;

        dst.ptr += inc * slotIndex;

        m_renderDevice.get_d3d12_device()->CopyDescriptorsSimple(
            1,
            dst, // dest (GPU ヒープ)
            src, // src  (CPU ヒープ)
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
} // namespace Drama::Graphics::DX12
