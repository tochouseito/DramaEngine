#pragma once

#include "AssetDataContainer.h"
#include "Model.h"

namespace Drama::Graphics::DX12
{
    class ResourceManager;
}

namespace Drama::Asset
{

    class AssetManager final
    {
    public:
        AssetManager(Drama::Graphics::DX12::ResourceManager& resourceManager)
            : m_resourceManager(resourceManager)
        {
        }
        ~AssetManager() = default;
    private:
        void create_default_assets();

        void create_cube_model();
    private:
        Drama::Graphics::DX12::ResourceManager& m_resourceManager;
        AssetDataContainer<ModelData> modelDataContainer;
    };
}
