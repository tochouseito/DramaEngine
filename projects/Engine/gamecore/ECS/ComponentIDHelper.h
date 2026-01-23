#pragma once
#include <typeinfo>

namespace Drama::ECS
{
    extern "C" __declspec(dllimport)
        size_t ecs_register_component_id(const char* uniqueName);

    template<class T>
    inline size_t component_id()
    {
        // 1) RTTI がオフなら #T の文字列などにする
        static size_t id = ecs_register_component_id(typeid(T).name());
        return id;
    }
}
