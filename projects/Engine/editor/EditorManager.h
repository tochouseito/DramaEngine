#pragma once

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
    private:
        bool m_Initialized = false;
    };
}
