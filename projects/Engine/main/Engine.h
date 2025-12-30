#pragma once
// c++ standard library
#include <memory>

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
        void Run();
    private:
        /// @brief 初期化
        /// @return 成功ならtrue
        [[nodiscard]]
        bool Initialize();
        /// @brief 終了処理
        void Shutdown();
        /// @brief 更新処理
        void Update();
        /// @brief 描画処理
        void Render();

        class Impl;
        std::unique_ptr<Impl> m_Impl;
        bool m_IsRunning = false;
    };
}
