#pragma once
#include "EventCommand.h"
#include <string>

namespace Drama::Core::Commands
{
    // コマンドの型宣言の置き場

    /*==================== WinApp ====================*/

    struct CmdShowWindow
    {
        char msg[256]{};
    };
    inline void ExecShowWindow(void*, const void* data)
    {
        const auto* cmd = static_cast<const CmdShowWindow*>(data);
    }

} // namespace Drama::Core::Commands
