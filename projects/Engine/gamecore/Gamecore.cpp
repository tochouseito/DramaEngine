#include "pch.h"
#include "Gamecore.h"
#include "gamecore/ECS/Components.h"
#include "gpuPipeline/GpuPipeline.h"

namespace Drama
{
    GameCore::GameCore()
        : m_ecsManager(std::make_unique<ECS::ECSManager>())
    {
        // 1) ECS マネージャーを生成する
        setup_ecs_manager();
    }
    void GameCore::test_function()
    {
        ECS::Entity ent = m_ecsManager->generate_entity();
        m_ecsManager->add_component<ECS::ObjectComponent>(ent);
        m_ecsManager->add_component<ECS::TransformComponent>(ent);
        m_testEntity = ent;
        auto& objWres = m_gpuPipeline->get_object_world_resource();
        auto& transformWres = m_gpuPipeline->get_transform_world_resource();
        ECS::ObjectComponent* objComp = m_ecsManager->get_component<ECS::ObjectComponent>(m_testEntity);
        ECS::TransformComponent* transComp = m_ecsManager->get_component<ECS::TransformComponent>(m_testEntity);
        objComp->mapped_data.resize(1);
        transComp->mapped_data.resize(1);
        objComp->id = objWres.allocate();
        transComp->id = transformWres.allocate();
        objWres.map(objComp->id, objComp->mapped_data);
        transformWres.map(transComp->id, transComp->mapped_data);
    }
    void GameCore::setup_ecs_manager()
    {

    }
} // namespace Drama
