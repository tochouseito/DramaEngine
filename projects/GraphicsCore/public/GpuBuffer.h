#pragma once
#include "stdafx.h"
#include "Core/Error/Result.h"
#include <typeindex>
#include <span>

namespace Drama::Graphics::DX12
{
    using Result = Core::Error::Result;

    class GpuResource
    {
    public:
        GpuResource() = default;
        virtual ~GpuResource() = default;
        /// @brief 破棄
        virtual void Destroy()
        {
            if (m_d3d12Resource)
            {
                m_d3d12Resource.Reset();
                m_d3d12Resource = nullptr;
            }
            m_currentState = D3D12_RESOURCE_STATE_COMMON;
        }
        /// @brief 直接リソースを取得する演算子
        ID3D12Resource* operator->() { return m_d3d12Resource.Get(); }
        const ID3D12Resource* operator->() const { return m_d3d12Resource.Get(); }
        /// @brief リソースを取得
        ID3D12Resource* GetResource() { return m_d3d12Resource.Get(); }
        ID3D12Resource** GetAddressOf() { return m_d3d12Resource.GetAddressOf(); }
        /// @brief リソースの使用状態を取得/設定
        D3D12_RESOURCE_STATES GetUseState() const { return m_currentState; }
        void SetUseState(D3D12_RESOURCE_STATES state) { m_currentState = state; }
        /// @brief フェンスとフェンス値を設定
        void SetFence(ID3D12Fence* pFence, uint64_t fenceValue)
        {
            m_pFence = pFence;
            m_FenceValue = fenceValue;
        }
        /// @brief リソースの使用状態を取得
        bool IsUsed() const
        {
            if (m_pFence)
            {
                return m_pFence->GetCompletedValue() < m_FenceValue;
            }
            return false;
        }
    protected:
        Result CreateResource(
            ID3D12Device& device,
            D3D12_HEAP_PROPERTIES& heapProperties,
            D3D12_HEAP_FLAGS heapFlags,
            D3D12_RESOURCE_DESC& desc,
            D3D12_RESOURCE_STATES InitialState,
            D3D12_CLEAR_VALUE* pClearValue,
            std::wstring_view name)
        {
            HRESULT hr = device.CreateCommittedResource(
                &heapProperties,
                heapFlags,
                &desc,
                InitialState,
                pClearValue,
                IID_PPV_ARGS(&m_d3d12Resource)
            );
            m_currentState = InitialState;
            if (FAILED(hr))
            {
                return Result::fail(
                    Core::Error::Facility::Graphics,
                    Core::Error::Code::CreationFailed,
                    Core::Error::Severity::Error,
                    static_cast<uint32_t>(hr),
                    "Failed to create D3D12 resource.");
            }
            SetD3D12Name(m_d3d12Resource.Get(), name.data());
        }
    protected:
        ComPtr<ID3D12Resource> m_d3d12Resource;
        D3D12_RESOURCE_STATES m_currentState = D3D12_RESOURCE_STATE_COMMON;
        ID3D12Fence* m_pFence = nullptr;///< リソースの同期用フェンス
        uint64_t m_FenceValue = 0;///< フェンス値
    };

    class GpuBuffer : public GpuResource
    {
    public:
        GpuBuffer() = default;
        virtual ~GpuBuffer() = default;

        virtual void Destroy() override
        {
            GpuResource::Destroy();
            m_BufferSize = 0;
            m_NumElements = 0;
            m_StructureByteStride = 0;
        }
        virtual std::type_index GetElementType() const noexcept { return typeid(void); }
        virtual std::type_index GetBufferType() const noexcept { return typeid(GpuBuffer); }
        UINT64 GetBufferSize() const noexcept { return m_BufferSize; }
        UINT GetNumElements() const noexcept { return m_NumElements; }
        UINT GetStructureByteStride() const noexcept { return m_StructureByteStride; }
    protected:
        Result CreateBuffer(
            ID3D12Device& device,
            D3D12_HEAP_PROPERTIES& heapProperties,
            D3D12_HEAP_FLAGS heapFlags,
            D3D12_RESOURCE_STATES InitialState,
            D3D12_RESOURCE_FLAGS resourceFlags,
            UINT numElements,
            UINT structureByteStride, std::wstring_view name)
        {
            // バッファのサイズを取得
            m_BufferSize = static_cast<UINT64>(numElements * structureByteStride);
            // 要素数を取得
            m_NumElements = numElements;
            // 要素のサイズを取得
            m_StructureByteStride = structureByteStride;
            // リソースの設定
            D3D12_RESOURCE_DESC resourceDesc{};
            resourceDesc.Width = m_BufferSize;// リソースのサイズ
            // バッファリソースの設定
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.Flags = resourceFlags;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return GpuResource::CreateResource(device, heapProperties, heapFlags, resourceDesc, InitialState, nullptr, name);
        }
    protected:
        UINT64 m_BufferSize = {};          ///< バッファサイズ
        UINT m_NumElements = {};           ///< 要素数
        UINT m_StructureByteStride = {};   ///< 構造体バイトストライド
    };

    template<typename T>
    class UploadBuffer : public GpuBuffer
    {
        public:
        UploadBuffer() = default;
        virtual ~UploadBuffer() = default;
        virtual void Destroy() override
        {
            if (GetResource())
            {
                GetResource()->Unmap(0, nullptr);
            }
            GpuBuffer::Destroy();
            m_MappedData = {};
        }
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        virtual std::type_index GetBufferType() const noexcept override { return typeid(UploadBuffer<T>); }
        Result Create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;// UploadHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::CreateBuffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride);
            // マッピング
            T* mappedData = nullptr;// 一時マップ用
            GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
            m_MappedData = std::span<T>(mappedData, numElements);
            // 0クリア
            memset(mappedData, 0, sizeof(T) * numElements);
            return Result::ok();
        }
        /// @brief マッピングデータ取得
        std::span<T>       GetMappedData() { return m_MappedData; }
        std::span<const T> GetMappedData() const { return m_MappedData; }
    private:
        std::span<T> m_MappedData;
    };

    template<typename T>
    class ReadbackBuffer : public GpuBuffer
    {
        public:
        ReadbackBuffer() = default;
        virtual ~ReadbackBuffer() = default;
        virtual void Destroy() override
        {
            if (GetResource())
            {
                GetResource()->Unmap(0, nullptr);
            }
            GpuBuffer::Destroy();
            m_MappedData = {};
        }
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        virtual std::type_index GetBufferType() const noexcept override { return typeid(ReadbackBuffer<T>); }
        Result Create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_READBACK;// ReadbackHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::CreateBuffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride);
            // マッピング
            T* mappedData = nullptr;// 一時マップ用
            GetResource()->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
            m_MappedData = std::span<T>(mappedData, numElements);
            return Result::ok();
        }
        /// @brief マッピングデータ取得
        std::span<T>       GetMappedData() { return m_MappedData; }
        std::span<const T> GetMappedData() const { return m_MappedData; }
    private:
        std::span<T> m_MappedData;
    };

    template<typename T>
    class ConstantBuffer : public GpuBuffer
    {
    public:
        /// @brief コンストラクタ
        ConstantBuffer() = default;
        /// @brief デストラクタ
        virtual ~ConstantBuffer() = default;
        /// @brief 破棄
        virtual void Destroy() override
        {
            GpuBuffer::Destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index GetBufferType() const noexcept override { return typeid(ConstantBuffer<T>); }
        /// @brief 作成
        Result CreateBuffer(ID3D12Device& device, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::CreateBuffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                1, structureByteStride, name);
            return Result::ok();
        }
    private:
    };

    template<typename T>
    class StructuredBuffer : public GpuBuffer
    {
    public:
        /// @brief コンストラクタ
        StructuredBuffer() = default;
        /// @brief デストラクタ
        virtual ~StructuredBuffer() = default;
        /// @brief 破棄
        virtual void Destroy() override
        {
            GpuBuffer::Destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index GetBufferType() const noexcept override { return typeid(StructuredBuffer<T>); }
        /// @brief 作成
        Result CreateBuffer(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::CreateBuffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride, name);
            return Result::ok();
        }
    private:
    };

    template<typename T>
    class RWStructuredBuffer : public GpuBuffer
    {
    public:
        /// @brief コンストラクタ
        RWStructuredBuffer() = default;
        /// @brief デストラクタ
        virtual ~RWStructuredBuffer() = default;
        /// @brief 破棄
        virtual void Destroy() override
        {
            GpuBuffer::Destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index GetBufferType() const noexcept override { return typeid(RWStructuredBuffer<T>); }
        /// @brief 作成
        Result CreateBuffer(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            GpuBuffer::CreateBuffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride, name);
            return Result::ok();
        }
    private:
    };

    template<typename T>
    class VertexBuffer : public GpuBuffer
    {
        public:
        /// @brief コンストラクタ
        VertexBuffer() = default;
        /// @brief デストラクタ
        virtual ~VertexBuffer() = default;
        /// @brief 破棄
        virtual void Destroy() override
        {
            GpuBuffer::Destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index GetBufferType() const noexcept override { return typeid(VertexBuffer<T>); }
        /// @brief 作成
        Result CreateBuffer(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlag = D3D12_RESOURCE_FLAG_NONE;
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = heapType;
            GpuBuffer::CreateBuffer(
                device, heapProperties, D3D12_HEAP_FLAG_NONE,
                resourceState, resourceFlag,
                numElements, structureByteStride, name);
            // 頂点バッファビューの設定
            m_View.BufferLocation = GetResource()->GetGPUVirtualAddress();
            m_View.SizeInBytes = static_cast<UINT>(GetResource()->GetDesc().Width);
            m_View.StrideInBytes = structureByteStride;
            return Result::ok();
        }
        /// @brief 頂点バッファビューを取得
        const D3D12_VERTEX_BUFFER_VIEW& GetView() const { return m_View; }
    private:
        D3D12_VERTEX_BUFFER_VIEW m_View{};
    };

    template<typename T>
    class IndexBuffer : public GpuBuffer
    {
        public:
        /// @brief コンストラクタ
        IndexBuffer() = default;
        /// @brief デストラクタ
        virtual ~IndexBuffer() = default;
        /// @brief 破棄
        virtual void Destroy() override
        {
            GpuBuffer::Destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index GetElementType() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index GetBufferType() const noexcept override { return typeid(IndexBuffer<T>); }
        /// @brief 作成
        Result CreateBuffer(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // Tが構造体、クラスの時、サイズチェック
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            // リソースのサイズ
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            // リソース用のヒープの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::CreateBuffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride);
            // インデックスバッファビューの設定
            m_View.BufferLocation = GetResource()->GetGPUVirtualAddress();
            m_View.SizeInBytes = static_cast<UINT>(GetResource()->GetDesc().Width);
            // インデックスの形式
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                m_View.Format = DXGI_FORMAT_R16_UINT;
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                m_View.Format = DXGI_FORMAT_R32_UINT;
            }
            else
            {
                static_assert(false, "IndexBuffer only supports uint16_t and uint32_t types.");
            }
            return Result::ok();
        }
        /// @brief インデックスバッファビューを取得
        const D3D12_INDEX_BUFFER_VIEW& GetView() const { return m_View; }
    private:
        D3D12_INDEX_BUFFER_VIEW m_View{};
    };

    class TextureBuffer : public GpuBuffer
    {
        public:
        TextureBuffer() = default;
        virtual ~TextureBuffer() = default;
        virtual void Destroy() override
        {
            GpuResource::Destroy();
            m_Desc = {};
        }
        const D3D12_RESOURCE_DESC& GetDesc() const { return m_Desc; }
        /// @brief 作成
        Result CreateTexture(ID3D12Device& device, D3D12_RESOURCE_DESC& desc, D3D12_CLEAR_VALUE* clearValue, std::wstring_view name)
        {
            // 利用するHeapの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            m_Desc = desc;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
            GpuResource::CreateResource(device, heapProperties, D3D12_HEAP_FLAG_NONE, desc, state, clearValue, name);
        }
    protected:
        D3D12_RESOURCE_DESC m_Desc{};
    };

    class DepthBuffer : public TextureBuffer
    {
        public:
        DepthBuffer() = default;
        virtual ~DepthBuffer() = default;
        virtual void Destroy() override
        {
            TextureBuffer::Destroy();
        }
        /// @brief 作成
        Result CreateDepthBuffer(ID3D12Device& device, D3D12_RESOURCE_DESC& desc, std::wstring_view name)
        {
            // 利用するHeapの設定
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            // DepthStencilとして使う通知
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            // 深度値のクリア設定
            D3D12_CLEAR_VALUE clearValue{};
            clearValue.DepthStencil.Depth = 1.0f;// 1.0f（最大値）でクリア
            clearValue.Format = desc.Format;// フォーマット。Resourceと合わせる
            return TextureBuffer::CreateTexture(device, desc, &clearValue, name);
        }
    };
} // namespace Drama::Graphics::DX12
