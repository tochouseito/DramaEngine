#include "pch.h"
#include "EditorManager.h"
#ifndef NDEBUG
#include <externals/imgui/include/imgui.h>

namespace Drama::Editor
{
    void EditorManager::render_ui()
    {
        ImGui::Begin("Demo");
        ImGui::Text("ImGui is running.");

        if (ImGui::Button("オブジェクト作成"))
        {
            // オブジェクト作成処理
            int i;
            i = 0;
        }

        ImGui::End();
    }
} // namespace Drama::Editor

#endif
