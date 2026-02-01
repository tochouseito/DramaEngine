#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace Drama::Asset
{
    template<typename T>
    struct AssetDataContainer
    {
        uint32_t add(std::string_view name, const T& data)
        {
            uint32_t index;
            if (!freeIndices.empty())
            {
                index = freeIndices.back();
                freeIndices.pop_back();
                assets[index] = data;
            }
            else
            {
                index = static_cast<uint32_t>(assets.size());
                assets.push_back(data);
            }
            assetIndexMap[name.data()] = index;
            return index;
        }

        void remove(std::string_view name)
        {
            if (assetIndexMap.contains(name.data()))
            {
                uint32_t index = assetIndexMap[name.data()];
                freeIndices.push_back(index);
                assetIndexMap.erase(name.data());
            }
        }

        void clear()
        {
            assets.clear();
            freeIndices.clear();
            assetIndexMap.clear();
        }

        void reserve(size_t size)
        {
            assets.reserve(size);
        }

        T* get(std::string_view name)
        {
            if (assetIndexMap.contains(name.data()))
            {
                uint32_t index = assetIndexMap[name.data()];
                return &assets[index];
            }
            return nullptr;
        }

        std::vector<T> assets; // アセットデータ配列
        std::vector<uint32_t> freeIndices; // 空きインデックス配列
        std::unordered_map<std::string, std::uint32_t> assetIndexMap; // アセット名からインデックスへのマップ
    };
}
