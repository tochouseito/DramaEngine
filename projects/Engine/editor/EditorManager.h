#pragma once

namespace Drama
{
    class GameCore;
}

namespace Drama::Editor
{
    class EditorManager final
    {
    public:
        /// @brief コンストラクタ
        EditorManager() = default;
        /// @brief デストラクタ
        ~EditorManager() = default;

        void render_ui();

        void set_game_core(Drama::GameCore* gameCore);
    private:
        bool m_Initialized = false;

        Drama::GameCore* m_gameCore = nullptr;
    };
}
