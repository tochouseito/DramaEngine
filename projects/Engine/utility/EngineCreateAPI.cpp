#include "pch.h"
#include "utility/EngineCreateAPI.h"
#ifdef ENGINE_CREATE_FUN
namespace Drama::API
{
    // Engineの生成
    DRAMA_API Drama::Engine* CreateEngine()
    {
        g_Engine = new Drama::Engine();
        return g_Engine;
    }
    // Engineの破棄
    DRAMA_API void DestroyEngine(Drama::Engine* engine)
    {
        if (engine)
        {
            delete engine;
            engine = nullptr;
            g_Engine = nullptr;
        }
    }
    // ポインタを受け取る
    DRAMA_API void SetEngine(Drama::Engine* engine)
    {
        g_Engine = engine;
    }
    // 稼働
    DRAMA_API void RunEngine()
    {
        if (g_Engine)
        {
            g_Engine->Run();
        }
    }
}
#endif
