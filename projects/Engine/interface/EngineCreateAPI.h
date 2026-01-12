#pragma once
#include "interface/ExportsMacro.h"
#include "main/Engine.h"
namespace Drama::API
{
#ifdef ENGINE_CREATE_FUN
    // Engineの生成
    DRAMA_API Drama::Engine* create_engine();
    // Engineの破棄
    DRAMA_API void destroy_engine(Drama::Engine* engine);
    // ポインタを受け取る
    DRAMA_API void set_engine(Drama::Engine* engine);
    // 稼働
    DRAMA_API void run_engine();
#endif
}
