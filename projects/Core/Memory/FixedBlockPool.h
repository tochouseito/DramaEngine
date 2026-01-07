#pragma once

// === C++ Standard Library ===
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <type_traits>

// === Engine ===
#include "Core/Error/Result.h"
#include "Core/Memory/LinearArena.h"

namespace Drama::Core::Memory
{
    template<class T>
    class FixedBlockPool final
    {
        static_assert(std::is_default_constructible_v<T>,
            "FixedBlockPool requires default constructible T.");
        static_assert(std::is_nothrow_destructible_v<T>,
            "FixedBlockPool requires nothrow destructible T.");

    public:
        FixedBlockPool() noexcept = default;

        ~FixedBlockPool() noexcept
        {
            // 1) 生成済みなら解放
            destroy();
        }

        Error::Result create(std::size_t capacity) noexcept
        {
            // 1) 二重生成防止
            if (m_nodes != nullptr)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidState,
                    Error::Severity::Error,
                    0,
                    "FixedBlockPool is already created.");
            }

            // 2) 引数チェック
            if (capacity == 0)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacity must be > 0.");
            }

            // 3) インデックス上限チェック（free list は uint32_t 前提）
            if (capacity > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()))
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacity is too large.");
            }

            // 4) 乗算オーバーフロー防止
            if (capacity > (std::numeric_limits<std::size_t>::max)() / sizeof(Node))
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacity is too large.");
            }

            // 5) 例外禁止なので nothrow で確保
            const std::size_t bytes = sizeof(Node) * capacity;
            void* mem = ::operator new(bytes, std::nothrow);
            if (mem == nullptr)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::OutOfMemory,
                    Error::Severity::Error,
                    0,
                    "FixedBlockPool allocation failed.");
            }

            // 6) 初期化（フリーリスト構築）
            m_nodes = static_cast<Node*>(mem);
            m_capacity = capacity;
            m_freeHead = 0;
            m_ownsMemory = true;

            for (std::size_t i = 0; i < capacity; ++i)
            {
                ::new (static_cast<void*>(&m_nodes[i])) Node();
                const uint32_t next =
                    (i + 1 < capacity) ? static_cast<uint32_t>(i + 1) : k_invalid;
                m_nodes[i].next = next;
            }

            return Error::Result::ok();
        }

        Error::Result create(LinearArena& arena, std::size_t capacity) noexcept
        {
            // 1) 二重生成防止
            if (m_nodes != nullptr)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidState,
                    Error::Severity::Error,
                    0,
                    "FixedBlockPool is already created.");
            }

            // 2) 引数チェック
            if (capacity == 0)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacity must be > 0.");
            }

            // 3) インデックス上限チェック（free list は uint32_t 前提）
            if (capacity > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()))
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacity is too large.");
            }

            // 4) 乗算オーバーフロー防止
            if (capacity > (std::numeric_limits<std::size_t>::max)() / sizeof(Node))
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacity is too large.");
            }

            // 5) メモリ確保（Arenaから）
            void* mem = nullptr;
            const std::size_t bytes = sizeof(Node) * capacity;
            const auto r = arena.try_allocate(mem, bytes, alignof(Node));
            if (!r)
            {
                return r;
            }

            // 6) 初期化（フリーリスト構築）
            m_nodes = static_cast<Node*>(mem);
            m_capacity = capacity;
            m_freeHead = 0;
            m_ownsMemory = false;

            for (std::size_t i = 0; i < capacity; ++i)
            {
                ::new (static_cast<void*>(&m_nodes[i])) Node();
                const uint32_t next =
                    (i + 1 < capacity) ? static_cast<uint32_t>(i + 1) : k_invalid;
                m_nodes[i].next = next;
            }

            return Error::Result::ok();
        }

        Error::Result try_alloc(T*& outPtr) noexcept
        {
            // 1) 未生成チェック
            if (m_nodes == nullptr)
            {
                outPtr = nullptr;
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidState,
                    Error::Severity::Error,
                    0,
                    "FixedBlockPool is not created.");
            }

            // 2) 空ならOOM
            if (m_freeHead == k_invalid)
            {
                outPtr = nullptr;
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::OutOfMemory,
                    Error::Severity::Error,
                    0,
                    "FixedBlockPool out of memory.");
            }

            // 3) 取り出す
            const uint32_t idx = m_freeHead;
            if (idx >= m_capacity)
            {
                outPtr = nullptr;
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidState,
                    Error::Severity::Error,
                    0,
                    "FixedBlockPool free list corrupted.");
            }
            Node& n = m_nodes[idx];
            m_freeHead = n.next;

            // 4) 構築して返す
            T* value = ::new (static_cast<void*>(&n.storage)) T();
            outPtr = value;
            return Error::Result::ok();
        }

        void free(T* p) noexcept
        {
            // 1) nullガード
            if (p == nullptr)
            {
                return;
            }

            if (m_nodes == nullptr || m_capacity == 0)
            {
                return;
            }

            // 2) 破棄して Node へ戻す
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                p->~T();
            }

            Node* n = reinterpret_cast<Node*>(p);
            if (n < m_nodes || n >= (m_nodes + m_capacity))
            {
                return;
            }

            // 3) フリーリストへpush
            const uint32_t idx = static_cast<uint32_t>(n - m_nodes);
            n->next = m_freeHead;
            m_freeHead = idx;
        }

        void destroy() noexcept
        {
            // 1) 未生成なら何もしない
            if (m_nodes == nullptr)
            {
                return;
            }

            // 2) メモリ解放（LinearArena経由は触らない）
            if (m_ownsMemory)
            {
                ::operator delete(m_nodes, std::nothrow);
            }

            m_nodes = nullptr;
            m_capacity = 0;
            m_freeHead = k_invalid;
            m_ownsMemory = false;
        }

        std::size_t capacity() const noexcept
        {
            return m_capacity;
        }

    private:
        struct Node final
        {
            std::aligned_storage_t<sizeof(T), alignof(T)> storage{};
            uint32_t next = k_invalid;
        };

        static constexpr uint32_t k_invalid = 0xFFFFFFFFu;

        Node* m_nodes = nullptr;
        std::size_t m_capacity = 0;
        uint32_t m_freeHead = k_invalid;
        bool m_ownsMemory = false;
    };
}
