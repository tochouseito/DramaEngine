#include "pch.h"
#include "interface/EngineCreateAPI.h"

#include <memory>
#ifdef ENGINE_CREATE_FUN
namespace
{
    // CreateEngineで生成した場合のみ所有する
    std::unique_ptr<Drama::Engine> s_ownedEngine;
    Drama::Engine* s_engine = nullptr;
}

namespace Drama::API
{
    // Engineの生成
    DRAMA_API Drama::Engine* CreateEngine()
    {
        s_ownedEngine = std::make_unique<Drama::Engine>();
        s_engine = s_ownedEngine.get();
        return s_engine;
    }
    // Engineの破棄
    DRAMA_API void DestroyEngine(Drama::Engine* engine)
    {
        if (!engine)
        {
            return;
        }

        if (engine == s_ownedEngine.get())
        {
            s_ownedEngine.reset();
            s_engine = nullptr;
            return;
        }

        if (engine == s_engine)
        {
            s_engine = nullptr;
        }
    }
    // ポインタを受け取る
    DRAMA_API void SetEngine(Drama::Engine* engine)
    {
        if (engine == s_ownedEngine.get())
        {
            s_engine = engine;
            return;
        }

        s_ownedEngine.reset();
        s_engine = engine;
    }
    // 稼働
    DRAMA_API void RunEngine()
    {
        if (s_engine)
        {
            s_engine->Run();
        }
    }
}
#endif
