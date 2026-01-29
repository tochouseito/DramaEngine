#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <memory>

// === Engine ===
#include "gamecore/ECS/ECSManager.h"

namespace Drama
{
    class GameCore final
    {
    public:
        GameCore();
        ~GameCore() = default;

        void test_function()
        {
            // 1) ECS マネージャーのテスト用関数を呼び出す
            // (実際の実装は ECSManager 側に記述する)
            (void)m_ecsManager;
        }
    private:
        std::unique_ptr<ECS::ECSManager> m_ecsManager = nullptr;
    };
}
