#pragma once

// === C++ Standard Library ===
#include <cstddef>
#include <cstdint>
#include <new>

// === Engine ===
#include "Core/Error/Result.h"

namespace Drama::Core::Memory
{
    class LinearArena final
    {
    public:
        LinearArena() noexcept = default;

        ~LinearArena() noexcept
        {
            // 1) 念のため解放
            destroy();
        }

        LinearArena(const LinearArena&) = delete;
        LinearArena& operator=(const LinearArena&) = delete;

        LinearArena(LinearArena&& rhs) noexcept
        {
            // 1) ムーブ
            *this = std::move(rhs);
        }

        LinearArena& operator=(LinearArena&& rhs) noexcept
        {
            // 1) 自己代入防止
            if (this != &rhs)
            {
                // 2) 既存を破棄
                destroy();

                // 3) 受け取り
                m_base = rhs.m_base;
                m_capacityBytes = rhs.m_capacityBytes;
                m_offsetBytes = rhs.m_offsetBytes;

                // 4) rhs無効化
                rhs.m_base = nullptr;
                rhs.m_capacityBytes = 0;
                rhs.m_offsetBytes = 0;
            }
            return *this;
        }

        Error::Result create(std::size_t capacityBytes) noexcept
        {
            // 1) 二重生成防止
            if (m_base != nullptr)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidState,
                    Error::Severity::Error,
                    0,
                    "LinearArena is already created.");
            }

            // 2) 引数チェック
            if (capacityBytes == 0)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "capacityBytes must be > 0.");
            }

            // 3) 例外禁止なので nothrow で確保
            void* p = ::operator new(capacityBytes, std::nothrow);
            if (p == nullptr)
            {
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::OutOfMemory,
                    Error::Severity::Error,
                    0,
                    "LinearArena allocation failed.");
            }

            // 4) 初期化
            m_base = static_cast<std::uint8_t*>(p);
            m_capacityBytes = capacityBytes;
            m_offsetBytes = 0;

            return Error::Result::ok();
        }

        void destroy() noexcept
        {
            // 1) 生成済みなら解放
            if (m_base != nullptr)
            {
                ::operator delete(m_base, std::nothrow);

                m_base = nullptr;
                m_capacityBytes = 0;
                m_offsetBytes = 0;
            }
        }

        void reset() noexcept
        {
            // 1) 先頭に戻す
            m_offsetBytes = 0;
        }

        Error::Result try_allocate(void*& outPtr, std::size_t bytes, std::size_t alignment) noexcept
        {
            // 1) 生成済みチェック
            if (m_base == nullptr)
            {
                outPtr = nullptr;
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidState,
                    Error::Severity::Error,
                    0,
                    "LinearArena is not created.");
            }

            // 2) 0バイトは許容（nullptr返し）
            if (bytes == 0)
            {
                outPtr = nullptr;
                return Error::Result::ok();
            }

            // 3) alignmentチェック（2の冪）
            if (alignment == 0 || (alignment & (alignment - 1)) != 0)
            {
                outPtr = nullptr;
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::InvalidArg,
                    Error::Severity::Error,
                    0,
                    "alignment must be power of two.");
            }

            // 4) アライン調整
            const std::size_t current = m_offsetBytes;
            const std::size_t aligned = (current + (alignment - 1)) & ~(alignment - 1);

            // 5) 容量チェック
            if (aligned + bytes > m_capacityBytes)
            {
                outPtr = nullptr;
                return Error::Result::fail(
                    Error::Facility::Core,
                    Error::Code::OutOfMemory,
                    Error::Severity::Error,
                    0,
                    "LinearArena out of memory.");
            }

            // 6) 返す
            outPtr = m_base + aligned;
            m_offsetBytes = aligned + bytes;

            return Error::Result::ok();
        }

        std::size_t capacity_bytes() const noexcept
        {
            return m_capacityBytes;
        }

        std::size_t used_bytes() const noexcept
        {
            return m_offsetBytes;
        }

        std::size_t remaining_bytes() const noexcept
        {
            return m_capacityBytes - m_offsetBytes;
        }

    private:
        std::uint8_t* m_base = nullptr;
        std::size_t m_capacityBytes = 0;
        std::size_t m_offsetBytes = 0;
    };
}
