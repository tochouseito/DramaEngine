#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <memory>

// === Engine ===
#include "gamecore/ECS/ECSManager.h"
#include "gamecore/GameWorld.h"

namespace Drama
{
    class GameCore final
    {
    public:
        GameCore();
        ~GameCore() = default;

        void test_function()
        {
            
        }
    private:
        void setup_ecs_manager();
    private:
        std::unique_ptr<ECS::ECSManager> m_ecsManager = nullptr;
        std::unique_ptr<GameWorld> m_gameWorld = nullptr;
    };
}
