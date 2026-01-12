#include "pch.h"
#include "interface/EngineCreateAPI.h"

#include <memory>
#ifdef ENGINE_CREATE_FUN
namespace
{
    // create_engineで生成した場合のみ所有する
    std::unique_ptr<Drama::Engine> ownedEngine;
    Drama::Engine* enginePtr = nullptr;
}

namespace Drama::API
{
    // Engineの生成
    DRAMA_API Drama::Engine* create_engine()
    {
        // 1) 所有権を内部に持つため unique_ptr で生成する
        // 2) 参照用の生ポインタを更新して返す
        ownedEngine = std::make_unique<Drama::Engine>();
        enginePtr = ownedEngine.get();
        return enginePtr;
    }
    // Engineの破棄
    DRAMA_API void destroy_engine(Drama::Engine* engine)
    {
        // 1) 無効ポインタは何もしない
        // 2) 所有している場合のみ解放し、参照を切る
        if (!engine)
        {
            return;
        }

        if (engine == ownedEngine.get())
        {
            ownedEngine.reset();
            enginePtr = nullptr;
            return;
        }

        if (engine == enginePtr)
        {
            enginePtr = nullptr;
        }
    }
    // ポインタを受け取る
    DRAMA_API void set_engine(Drama::Engine* engine)
    {
        // 1) 所有しているインスタンスなら参照のみ更新する
        // 2) 外部提供なら所有権を手放して参照を更新する
        if (engine == ownedEngine.get())
        {
            enginePtr = engine;
            return;
        }

        ownedEngine.reset();
        enginePtr = engine;
    }
    // 稼働
    DRAMA_API void run_engine()
    {
        // 1) 有効なエンジンがある場合のみ起動する
        if (enginePtr)
        {
            enginePtr->run();
        }
    }
}
#endif
