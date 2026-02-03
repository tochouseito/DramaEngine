#pragma once

// === C++ Standard Library ===
#include <cstdint>
#include <memory>

// === Engine ===
#include "gamecore/ECS/ECSManager.h"
#include "gamecore/GameWorld.h"

namespace Drama
{
    namespace Graphics
    {
        class GpuPipeline;
    }

    class GameCore final
    {
    public:
        GameCore();
        ~GameCore() = default;

        void test_function();
        void set_gpu_pipeline(Graphics::GpuPipeline* pipeline)
        {
            m_gpuPipeline = pipeline;
        }
    private:
        void setup_ecs_manager();
    private:
        std::unique_ptr<ECS::ECSManager> m_ecsManager = nullptr;
        std::unique_ptr<GameWorld> m_gameWorld = nullptr;

        ECS::Entity m_testEntity{};
        Graphics::GpuPipeline* m_gpuPipeline = nullptr;
    };
}
