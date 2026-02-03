#pragma once

#include <vector>

#include "gamecore/ECS/ECSManager.h"
#include "Math/public/Vector2.h"
#include "Math/public/Vector3.h"
#include "Math/public/Vector4.h"
#include "Math/public/Scale.h"
#include "Math/public/Quaternion.h"
#include "Math/public/Matrix4.h"
#include "gpuPipeline/worldResources/ObjectWorldResource.h"
#include "gpuPipeline/worldResources/TransformWorldResource.h"

namespace Drama::ECS
{
    struct ObjectComponent : public IComponentTag
    {

    public:
        uint32_t object_id = 0;
        uint32_t visible = 0;
        uint32_t model_id = 0;
        uint32_t transform_id = 0;
    //private:
        uint32_t id = 0;
        std::vector<Graphics::ObjectData*> mapped_data;
    };

    struct TransformComponent : public IComponentTag
    {
    public:
        Math::float3 position = Math::float3::zero();
        Math::float3 rotation = Math::float3::zero();
        Math::float3 scale = Math::float3::zero();
    //private:
        uint32_t id = 0;
        std::vector<Graphics::TransformData*> mapped_data;
    };
}
