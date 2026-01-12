#pragma once
// c++ standard library
#include <memory>
#include <functional>

namespace Drama
{
    class Engine
    {
    public:
        /// @brief コンストラクタ
        explicit Engine();
        /// @brief デストラクタ
        ~Engine();
        /// @brief 稼働
        void run();
    private:
        /// @brief 初期化
        /// @return 成功ならtrue
        [[nodiscard]]
        bool initialize();
        /// @brief 終了処理
        void shutdown();
        /// @brief 更新処理
        std::function<void(uint64_t, uint32_t)> update();
        /// @brief 描画処理
        std::function<void(uint64_t, uint32_t)> render();
        /// @brief フリップ処理
        std::function<void(uint64_t, uint32_t)> present();

        class Impl;
        std::unique_ptr<Impl> m_impl;
        bool m_isRunning = false;
    };
}
