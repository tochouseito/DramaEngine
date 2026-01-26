#include "pch.h"
#include "EditorManager.h"
#ifndef NDEBUG
#include <externals/imgui/include/imgui.h>

namespace Drama::Editor
{
    void EditorManager::render_ui()
    {
        ImGui::Begin("Editor");
        ImGui::Text("ImGui is running.");
        ImGui::End();
    }
} // namespace Drama::Editor

#endif
