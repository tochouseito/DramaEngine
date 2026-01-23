#include "pch.h"
#include <unordered_map>
#include <string>
#include <mutex>

namespace Drama::ECS
{
    extern "C" __declspec(dllexport)
        size_t ecs_register_component_id(const char* uniqueName)   // C で export すると楽
    {
        // 1) 型名の登録テーブルと採番カウンタを保護しながら更新する
        static std::unordered_map<std::string, size_t> idByName;
        static size_t nextId = 0;
        static std::mutex mutex;
        std::lock_guard lock(mutex);
        auto [it, inserted] = idByName.try_emplace(uniqueName, nextId);
        if (inserted)
        {
            ++nextId; // 新規登録なら採番
        }
        return it->second; // 既存なら同じ Id を返す
    }
}
