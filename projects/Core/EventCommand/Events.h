#pragma once
#include"EventCommand.h"
#include <string>

namespace Drama::Core::Events
{
    // イベントの型宣言の置き場

    /*==================== System Init Events ====================*/

    /*==================== WinApp ====================*/

    /// @brief ウィンドウ表示イベント
    struct EveShowWindow
    {
        const char* msg{};
    };

} // namespace Drama::Core::Events
