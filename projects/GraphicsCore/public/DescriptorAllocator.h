#pragma once
#include "stdafx.h"

// C++ standard includes
#include <vector>
#include <array>

// Drama includes
#include "GpuBuffer.h"

namespace Drama::Graphics::DX12
{
    // 前方宣言
    class RenderDevice;

    /// @brief ディスクリプタヒープの種類
    enum class HeapType : uint8_t
    {
        CBV_SRV_UAV,
        SAMPLER,
        RTV,
        DSV,
        kCount
    };

    class DescriptorAllocator final
    {
    public:
        /// @brief ディスクリプタテーブルの種類
        enum class TableKind : uint8_t
        {
            Textures,
            Buffers,
            RenderTargets,
            DepthStencils
        };

        /// @brief テーブル ID
        struct TableID final
        {
            TableKind kind = TableKind::Buffers;
            uint16_t  generation{};  ///< テーブルの世代（将来の再配置用）
            uint32_t  index{};       ///< テーブル内のローカル index（0..capacity-1）

            static constexpr uint32_t Invalid = 0xFFFFFFFF;

            bool valid() const { return index != Invalid; }
        };

        DescriptorAllocator(RenderDevice& device)
            : m_device(device)
        {
        }
        ~DescriptorAllocator() = default;

        /// @brief 初期化
        [[nodiscard]]
        bool Initialize(
            uint32_t texCap,
            uint32_t bufCap,
            uint32_t rtCap = 32,
            uint32_t dsCap = 2);

        /// @brief テーブル別の割り当て／解放
        TableID Allocate(TableKind k);
        void    Free(const TableID& id);

        /// @brief 各種 View 作成
        void CreateCBV(TableID& id, GpuBuffer* buf);
        void CreateSRVBuffer(TableID& id, GpuBuffer* buf);
        void CreateUAVBuffer(TableID& id, GpuBuffer* buf);
        void CreateUAVRawBuffer(TableID& id, GpuBuffer* buf);

        void CreateSRVTexture2D(TableID& id, GpuResource* res);
        void CreateRTV(TableID& id, GpuResource* res);
        void CreateDSV(TableID& id, GpuResource* res);

        /// @brief GPU / CPU ハンドル取得
        D3D12_GPU_DESCRIPTOR_HANDLE GetTableBaseGPU(TableKind k);
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(TableID id);
        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(TableID id);

        /// @brief ディスクリプタヒープ取得
        ID3D12DescriptorHeap* GetDescriptorHeap(HeapType type) const noexcept
        {
            if (type == HeapType::CBV_SRV_UAV)
            {
                // CBV_SRV_UAV は GPU 可視ヒープを返す
                return m_GpuSUVHeap.Get();
            }
            return m_DescriptorHeaps[static_cast<size_t>(type)].Get();
        }
    private:
        /// @brief テーブル内部情報
        struct Table final
        {
            uint32_t baseIndex = 0;            ///< ヒープ内の先頭スロット
            uint32_t capacity = 0;
            uint16_t generation = 0;
            std::vector<uint32_t> freeList;    ///< 空きスロット
            HeapType heapType = HeapType::CBV_SRV_UAV;
        };
        /// @brief 旧ヒープの寿命管理（GPU 完了まで保持）※実装予定
        struct RetiredHeap
        {
            ComPtr<ID3D12DescriptorHeap> heap;
            uint64_t fence = 0;
        };
        /// @brief 足りなければ拡張（実装予定）
        void EnsureCapacity(TableKind k, uint32_t needOneMore);

        /// @brief 再配置（実装予定）
        void RecreateHeap(TableKind k, uint32_t newCap, uint32_t newBufCap = 0);

        /// @brief テーブル取得
        Table& GetTable(TableKind k);

        /// @brief CBV_SRV_UAV テーブルのディスクリプタを GPU 可視ヒープへコピー
        void CopyToGpuHeap(const TableID& id);
    private:
        RenderDevice& m_device;
        /// @brief ヒープタイプごとのディスクリプタサイズ
        std::array<UINT, static_cast<size_t>(HeapType::kCount)> m_DescriptorSizes = { 0 };

        Table m_Textures;
        Table m_Buffers;
        Table m_RenderTargets;
        Table m_DepthStencils;
        std::vector<RetiredHeap> m_retired;

        // 各種ディスクリプタヒープ初期サイズ
        static constexpr uint32_t kMaxSUVDescriptorHeapSize = 1024;
        static constexpr uint32_t kMaxRTVDescriptorHeapSize = 32;
        static constexpr uint32_t kMaxDSVDescriptorHeapSize = 2;

        /// @brief CPU 専用ディスクリプタヒープ（タイプごと）
        std::array<ComPtr<ID3D12DescriptorHeap>, static_cast<size_t>(HeapType::kCount)> m_DescriptorHeaps{};

        /// @brief CBV_SRV_UAV 用 GPU 可視ヒープ
        ComPtr<ID3D12DescriptorHeap> m_GpuSUVHeap = nullptr;
    };
}
