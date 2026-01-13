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
        virtual void destroy()
        {
            // 1) 参照を切ってリソース破棄を確実にする
            // 2) 以後の誤使用を防ぐため状態を初期化する
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
        ID3D12Resource* get_resource() { return m_d3d12Resource.Get(); }
        ID3D12Resource** get_address_of() { return m_d3d12Resource.GetAddressOf(); }
        /// @brief リソースの使用状態を取得/設定
        D3D12_RESOURCE_STATES get_use_state() const { return m_currentState; }
        void set_use_state(D3D12_RESOURCE_STATES state) { m_currentState = state; }
        /// @brief フェンスとフェンス値を設定
        void set_fence(ID3D12Fence* fence, uint64_t fenceValue)
        {
            // 1) 外部同期の完了判定に必要な情報を保持する
            // 2) 参照先は非所有なのでポインタのみ保持する
            m_fence = fence;
            m_fenceValue = fenceValue;
        }
        /// @brief リソースの使用状態を取得
        bool is_used() const
        {
            // 1) フェンスがある場合のみ完了値を参照する
            // 2) 未完了なら使用中として扱う
            if (m_fence)
            {
                return m_fence->GetCompletedValue() < m_fenceValue;
            }
            return false;
        }
    protected:
        Result create_resource(
            ID3D12Device& device,
            D3D12_HEAP_PROPERTIES& heapProperties,
            D3D12_HEAP_FLAGS heapFlags,
            D3D12_RESOURCE_DESC& desc,
            D3D12_RESOURCE_STATES initialState,
            D3D12_CLEAR_VALUE* clearValue,
            std::wstring_view name)
        {
            // 1) 設定に従ってリソースを生成する
            // 2) 失敗時は Result で通知する
            HRESULT hr = device.CreateCommittedResource(
                &heapProperties,
                heapFlags,
                &desc,
                initialState,
                clearValue,
                IID_PPV_ARGS(&m_d3d12Resource)
            );
            m_currentState = initialState;
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
            return Result::ok();
        }
    protected:
        ComPtr<ID3D12Resource> m_d3d12Resource;
        D3D12_RESOURCE_STATES m_currentState = D3D12_RESOURCE_STATE_COMMON;
        ID3D12Fence* m_fence = nullptr;///< リソースの同期用フェンス
        uint64_t m_fenceValue = 0;///< フェンス値
    };

    class GpuBuffer : public GpuResource
    {
    public:
        GpuBuffer() = default;
        virtual ~GpuBuffer() = default;

        virtual void destroy() override
        {
            // 1) 基底の破棄を先に実行する
            // 2) 参照用のサイズ情報を初期化する
            GpuResource::destroy();
            m_bufferSize = 0;
            m_numElements = 0;
            m_structureByteStride = 0;
        }
        virtual std::type_index get_element_type() const noexcept { return typeid(void); }
        virtual std::type_index get_buffer_type() const noexcept { return typeid(GpuBuffer); }
        UINT64 get_buffer_size() const noexcept { return m_bufferSize; }
        UINT get_num_elements() const noexcept { return m_numElements; }
        UINT get_structure_byte_stride() const noexcept { return m_structureByteStride; }
    protected:
        Result create_buffer(
            ID3D12Device& device,
            D3D12_HEAP_PROPERTIES& heapProperties,
            D3D12_HEAP_FLAGS heapFlags,
            D3D12_RESOURCE_STATES initialState,
            D3D12_RESOURCE_FLAGS resourceFlags,
            UINT numElements,
            UINT structureByteStride, std::wstring_view name)
        {
            // 1) バッファ情報を保持して後続のビュー生成に備える
            // 2) バッファ用のリソースを生成する
            m_bufferSize = static_cast<UINT64>(numElements * structureByteStride);
            m_numElements = numElements;
            m_structureByteStride = structureByteStride;
            D3D12_RESOURCE_DESC resourceDesc{};
            resourceDesc.Width = m_bufferSize;// リソースのサイズ
            // バッファリソースの設定
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.Flags = resourceFlags;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            return GpuResource::create_resource(device, heapProperties, heapFlags, resourceDesc, initialState, nullptr, name);
        } 
    protected:
        UINT64 m_bufferSize = {};          ///< バッファサイズ
        UINT m_numElements = {};           ///< 要素数
        UINT m_structureByteStride = {};   ///< 構造体バイトストライド
    };

    template<typename T>
    class UploadBuffer : public GpuBuffer
    {
        public:
        UploadBuffer() = default;
        virtual ~UploadBuffer() = default;
        virtual void destroy() override
        {
            // 1) マップ中ならアンマップして安全にする
            // 2) 基底の破棄を行って状態を初期化する
            if (get_resource())
            {
                get_resource()->Unmap(0, nullptr);
            }
            GpuBuffer::destroy();
            m_mappedData = {};
        }
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(UploadBuffer<T>); }
        Result create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) アップロード用のリソースを生成してマップする
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;// UploadHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::create_buffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride, name);
            T* mappedData = nullptr;// 一時マップ用
            get_resource()->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
            m_mappedData = std::span<T>(mappedData, numElements);
            memset(mappedData, 0, sizeof(T) * numElements);
            return Result::ok();
        }
        /// @brief マッピングデータ取得
        std::span<T>       get_mapped_data() { return m_mappedData; }
        std::span<const T> get_mapped_data() const { return m_mappedData; }
    private:
        std::span<T> m_mappedData;
    };

    template<typename T>
    class ReadbackBuffer : public GpuBuffer
    {
        public:
        ReadbackBuffer() = default;
        virtual ~ReadbackBuffer() = default;
        virtual void destroy() override
        {
            // 1) マップ中ならアンマップして安全にする
            // 2) 基底の破棄を行って状態を初期化する
            if (get_resource())
            {
                get_resource()->Unmap(0, nullptr);
            }
            GpuBuffer::destroy();
            m_mappedData = {};
        }
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(ReadbackBuffer<T>); }
        Result create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) リードバック用のリソースを生成してマップする
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_READBACK;// ReadbackHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::create_buffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride, name);
            T* mappedData = nullptr;// 一時マップ用
            get_resource()->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
            m_mappedData = std::span<T>(mappedData, numElements);
            return Result::ok();
        }
        /// @brief マッピングデータ取得
        std::span<T>       get_mapped_data() { return m_mappedData; }
        std::span<const T> get_mapped_data() const { return m_mappedData; }
    private:
        std::span<T> m_mappedData;
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
        virtual void destroy() override
        {
            // 1) 基底の破棄を実行する
            GpuBuffer::destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(ConstantBuffer<T>); }
        /// @brief 作成
        Result create(ID3D12Device& device, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) 定数バッファ用のリソースを生成する
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::create_buffer(
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
        virtual void destroy() override
        {
            // 1) 基底の破棄を実行する
            GpuBuffer::destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(StructuredBuffer<T>); }
        /// @brief 作成
        Result create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) StructuredBuffer 用のリソースを生成する
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::create_buffer(
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
        virtual void destroy() override
        {
            // 1) 基底の破棄を実行する
            GpuBuffer::destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(RWStructuredBuffer<T>); }
        /// @brief 作成
        Result create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) UAV 用リソースを生成する
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            GpuBuffer::create_buffer(
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
        virtual void destroy() override
        {
            // 1) 基底の破棄を実行する
            GpuBuffer::destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(VertexBuffer<T>); }
        /// @brief 作成
        Result create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) 頂点バッファとビューを生成する
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
            D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlag = D3D12_RESOURCE_FLAG_NONE;
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = heapType;
            GpuBuffer::create_buffer(
                device, heapProperties, D3D12_HEAP_FLAG_NONE,
                resourceState, resourceFlag,
                numElements, structureByteStride, name);
            m_view.BufferLocation = get_resource()->GetGPUVirtualAddress();
            m_view.SizeInBytes = static_cast<UINT>(get_resource()->GetDesc().Width);
            m_view.StrideInBytes = structureByteStride;
            return Result::ok();
        }
        /// @brief 頂点バッファビューを取得
        const D3D12_VERTEX_BUFFER_VIEW& get_view() const { return m_view; }
    private:
        D3D12_VERTEX_BUFFER_VIEW m_view{};
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
        virtual void destroy() override
        {
            // 1) 基底の破棄を実行する
            GpuBuffer::destroy();
        }
        /// @brief 要素の型を取得
        virtual std::type_index get_element_type() const noexcept override { return typeid(T); }
        /// @brief バッファの型を取得
        virtual std::type_index get_buffer_type() const noexcept override { return typeid(IndexBuffer<T>); }
        /// @brief 作成
        Result create(ID3D12Device& device, UINT numElements, std::wstring_view name)
        {
            // 1) 16 バイト境界を守るためサイズ条件を検証する
            // 2) インデックスバッファとビューを生成する
            static_assert(!std::is_class_v<T> || sizeof(T) % 16 == 0, "The size of T must be a multiple of 16 bytes.");
            UINT structureByteStride = static_cast<UINT>(sizeof(T));
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
            D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
            D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
            GpuBuffer::create_buffer(
                device, heapProperties, heapFlags,
                initialState, resourceFlags,
                numElements, structureByteStride, name);
            m_view.BufferLocation = get_resource()->GetGPUVirtualAddress();
            m_view.SizeInBytes = static_cast<UINT>(get_resource()->GetDesc().Width);
            if constexpr (std::is_same_v<T, uint16_t>)
            {
                m_view.Format = DXGI_FORMAT_R16_UINT;
            }
            else if constexpr (std::is_same_v<T, uint32_t>)
            {
                m_view.Format = DXGI_FORMAT_R32_UINT;
            }
            else
            {
                static_assert(false, "IndexBuffer only supports uint16_t and uint32_t types.");
            }
            return Result::ok();
        }
        /// @brief インデックスバッファビューを取得
        const D3D12_INDEX_BUFFER_VIEW& get_view() const { return m_view; }
    private:
        D3D12_INDEX_BUFFER_VIEW m_view{};
    };

    class TextureBuffer : public GpuResource
    {
        public:
        TextureBuffer() = default;
        virtual ~TextureBuffer() = default;
        virtual void destroy() override
        {
            // 1) 基底の破棄を行う
            // 2) 保持している記述子情報を初期化する
            GpuResource::destroy();
            m_desc = {};
        }
        const D3D12_RESOURCE_DESC& get_desc() const { return m_desc; }
        /// @brief 作成
        Result create_texture(ID3D12Device& device, D3D12_RESOURCE_DESC& desc, D3D12_CLEAR_VALUE* clearValue, std::wstring_view name)
        {
            // 1) テクスチャ用のヒープ設定を決める
            // 2) リソースを生成して記述子を保持する
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            m_desc = desc;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
            GpuResource::create_resource(device, heapProperties, D3D12_HEAP_FLAG_NONE, desc, state, clearValue, name);
        }
    protected:
        D3D12_RESOURCE_DESC m_desc{};
    };

    class DepthBuffer : public TextureBuffer
    {
        public:
        DepthBuffer() = default;
        virtual ~DepthBuffer() = default;
        virtual void destroy() override
        {
            // 1) TextureBuffer の破棄に委譲する
            TextureBuffer::destroy();
        }
        /// @brief 作成
        Result create(ID3D12Device& device, D3D12_RESOURCE_DESC& desc, std::wstring_view name)
        {
            // 1) Depth 用のフラグとクリア値を設定する
            // 2) DepthBuffer を生成して返す
            D3D12_HEAP_PROPERTIES heapProperties{};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;// DefaultHeapを使う
            // DepthStencilとして使う通知
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            // 深度値のクリア設定
            D3D12_CLEAR_VALUE clearValue{};
            clearValue.DepthStencil.Depth = 1.0f;// 1.0f（最大値）でクリア
            clearValue.Format = desc.Format;// フォーマット。Resourceと合わせる
            return TextureBuffer::create_texture(device, desc, &clearValue, name);
        }
    };
} // namespace Drama::Graphics::DX12
