#pragma once
// === C++ Standard Library ===
#include <iostream>
#include <vector>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <any>
#include <mutex>
#include <optional>

namespace Drama::Core
{
    namespace EventCommand
    {
        ///-----------------------------------------
        /// Event（型安全 Pub-Sub：同期簡易版）
        ///-----------------------------------------
        class EventSystem final
        {
        public:
            template<class E>
            using Handler = std::function<void(const E&)>;

            template<class E>
            void Subscribe(Handler<E> h)
            {
                auto& vec = getVec<E>();
                vec.push_back(std::move(h));
            }

            template<class E>
            void Publish(const E& e)
            {
                auto& vec = getVec<E>();
                for (auto& h : vec) h(e);
            }

        private:
            // any 値でハンドラベクタを保持（手書き new/delete なし）
            std::unordered_map<std::type_index, std::any> m_Handlers;

            template<class E>
            std::vector<Handler<E>>& getVec()
            {
                const std::type_index k{ typeid(E) };
                auto [it, inserted] = m_Handlers.try_emplace(k, std::vector<Handler<E>>{});
                return *std::any_cast<std::vector<Handler<E>>>(&(it->second));
            }
        };

        ///-----------------------------------------
        /// Command（POD + ExecFn）
        ///-----------------------------------------
        using ExecFn = void(*)(void* ctx, const void* data);

        struct CmdEntry
        {
            ExecFn exec{};
            std::vector<uint8_t> payload; // POD をそのまま詰める
        };

        class CommandBuffer final
        {
        public:
            template<class T>
            void Push(ExecFn fn, const T& pod)
            {
                static_assert(std::is_trivially_copyable_v<T>, "payload must be trivially copyable");
                CmdEntry e; e.exec = fn;
                e.payload.resize(sizeof(T));
                std::memcpy(e.payload.data(), &pod, sizeof(T));
                cmds_.push_back(std::move(e));
            }

            // 汎用実行：ctx は Executor 側のコンテキスト
            void ExecuteAll(void* ctx)
            {
                for (auto& c : cmds_) c.exec(ctx, c.payload.data());
                cmds_.clear();
            }

            bool Empty() const { return cmds_.empty(); }

        private:
            std::vector<CmdEntry> cmds_;
        };

        ///-----------------------------------------
        /// ExecutorHub：各キューの実行方法を登録し一括実行
        ///-----------------------------------------
        class ExecutorHub final
        {
        public:
            // buf をどう実行するか（ctx 生成も含めて）を渡す
            void Register(CommandBuffer& buf, std::function<void(CommandBuffer&)> run)
            {
                m_Entries.push_back({ &buf, std::move(run) });
            }

            // 1フレーム分を一括実行（登録順）
            void ExecuteAll()
            {
                for (auto& e : m_Entries)
                {
                    if (!e.buf) continue;
                    e.run(*e.buf);
                }
            }

            bool HasPendingCommands() const
            {
                for (auto& e : m_Entries)
                {
                    if (e.buf && !e.buf->Empty())
                    {
                        return true;
                    }
                }
                return false;
            }

        private:
            struct Entry
            {
                CommandBuffer* buf{};
                std::function<void(CommandBuffer&)> run;
            };
            std::vector<Entry> m_Entries;
        };

        ///-----------------------------------------
        /// Router 基底
        ///-----------------------------------------
        class IRouter
        {
        public:
            IRouter(EventSystem& e, CommandBuffer& c)
                : m_EventSystem(e), m_CommandBuffer(c)
            {
            }
            virtual ~IRouter() = default;
            virtual void Flush() = 0;

        protected:
            template<class E, class F>
            void Subscribe(F&& f)
            {
                m_EventSystem.Subscribe<E>(std::forward<F>(f));
            }

            EventSystem& m_EventSystem;
            CommandBuffer& m_CommandBuffer;
        };

        ///-----------------------------------------
        /// RouterHub
        ///-----------------------------------------
        class RouterHub final
        {
        public:
            template<class T, class...Args>
            T& Add(Args&&...args)
            {
                auto up = std::make_unique<T>(std::forward<Args>(args)...);
                auto& ref = *up; routers_.push_back(std::move(up)); return ref;
            }
            void FlushAll() { for (auto& r : routers_) r->Flush(); }
        private:
            std::vector<std::unique_ptr<IRouter>> routers_;
        };

        ///-----------------------------------------
        /// ★ Orchestrator 用コンテキスト（方式①）
        ///  - 複数システムにまたがるコマンドは、この Ctx 1個で実行
        ///  - 任意の“チャネル（ドメイン）”に対して ctx/queue を登録できる
        ///-----------------------------------------
        enum class Channel : uint8_t
        {
            Render = 0,
            Asset = 1,
            User0 = 2,
            User1 = 3,
            Max = 8
        };

        class OrchestratorContext final
        {
        public:
            template<Channel C>
            void SetContext(void* p) noexcept
            {
                ctx_[static_cast<size_t>(C)] = p;
            }
            template<Channel C>
            void* GetContext() const noexcept
            {
                return ctx_[static_cast<size_t>(C)];
            }

            template<Channel C>
            void SetQueue(CommandBuffer& q) noexcept
            {
                queues_[static_cast<size_t>(C)] = &q;
            }
            template<Channel C>
            CommandBuffer& Queue() const
            {
                auto* q = queues_[static_cast<size_t>(C)];
                assert(q && "Queue not registered for this channel");
                return *q;
            }

        private:
            void* ctx_[static_cast<size_t>(Channel::Max)]{};          // 各チャネルの実行用 ctx（任意型）
            CommandBuffer* queues_[static_cast<size_t>(Channel::Max)]{};      // 他キューへの橋渡し
        };

    } // namespace EventCommand

    // イベント/コマンド/ルータの「型宣言の置き場」は空でOK（実体はテスト側で定義）
    namespace Events {};
    namespace Commands {};
    namespace Routers {};

} // namespace Theatria::Core
