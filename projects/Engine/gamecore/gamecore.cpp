#include "pch.h"
#include "gamecore.h"

namespace Drama
{
    GameCore::GameCore()
        : m_ecsManager(std::make_unique<ECS::ECSManager>())
    {
        // 1) ECS マネージャーを生成する
    }
} // namespace Drama
