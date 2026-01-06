#pragma once

// === C++ Standard Library ===
#include <cstddef>
#include <cstdint>
#include <type_traits>

// === Engine ===
#include "Core/Error/Result.h"
#include "Core/Memory/LinearArena.h"

namespace Drama::Core::Memory
{
    template<class T>
    class FixedBlockPool final
    {
        static_assert(std::is_trivially_destructible_v<T>,
            "FixedBlockPool requires trivially destructible T.");

    public:
        FixedBlockPool() noexcept = default;

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

            // 3) メモリ確保（Arenaから）
            void* mem = nullptr;
            const std::size_t bytes = sizeof(Node) * capacity;
            const auto r = arena.try_allocate(mem, bytes, alignof(Node));
            if (!r)
            {
                return r;
            }

            // 4) 初期化（フリーリスト構築）
            m_nodes = static_cast<Node*>(mem);
            m_capacity = capacity;
            m_freeHead = 0;

            for (std::size_t i = 0; i < capacity; ++i)
            {
                m_nodes[i].next = static_cast<uint32_t>(i + 1);
            }
            m_nodes[capacity - 1].next = k_invalid;

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
            Node& n = m_nodes[idx];
            m_freeHead = n.next;

            // 4) 返す
            outPtr = &n.value;
            return Error::Result::ok();
        }

        void free(T* p) noexcept
        {
            // 1) nullガード
            if (p == nullptr)
            {
                return;
            }

            // 2) Nodeへ戻す（valueが先頭にある前提）
            Node* n = reinterpret_cast<Node*>(p);

            // 3) フリーリストへpush
            const uint32_t idx = static_cast<uint32_t>(n - m_nodes);
            n->next = m_freeHead;
            m_freeHead = idx;
        }

        std::size_t capacity() const noexcept
        {
            return m_capacity;
        }

    private:
        struct Node final
        {
            T value{};
            uint32_t next = k_invalid;
        };

        static constexpr uint32_t k_invalid = 0xFFFFFFFFu;

        Node* m_nodes = nullptr;
        std::size_t m_capacity = 0;
        uint32_t m_freeHead = k_invalid;
    };
}
