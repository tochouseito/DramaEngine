#include "pch.h"
#include "EditorManager.h"
#ifndef NDEBUG
#include <externals/imgui/include/imgui.h>
#include "gamecore/gamecore.h"

namespace Drama::Editor
{
    void EditorManager::render_ui()
    {
        ImGui::Begin("Demo");
        ImGui::Text("ImGui is running.");

        if (ImGui::Button("オブジェクト作成"))
        {
            // オブジェクト作成処理
            m_gameCore->test_function();
        }

        ImGui::End();
    }
    void EditorManager::set_game_core(Drama::GameCore* gameCore)
    {
        m_gameCore = gameCore;
    }
} // namespace Drama::Editor

#endif
