#pragma once
#include "utility/ExportsMacro.h"
#include "main/Engine.h"
namespace Drama::API
{
#ifdef ENGINE_CREATE_FUN
    // Engineの生成
    DRAMA_API Drama::Engine* CreateEngine();
    // Engineの破棄
    DRAMA_API void DestroyEngine(Drama::Engine* engine);
    // ポインタを受け取る
    DRAMA_API void SetEngine(Drama::Engine* engine);
    // Engineのポインタ
    static Drama::Engine* g_Engine = nullptr;
    // 稼働
    DRAMA_API void RunEngine();
#endif
}
