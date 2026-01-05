#pragma once
#include "Core/PlatformInterface/ILogger.h"

namespace Drama::Platform::Win
{
    class WinLogger final : public Drama::Core::IO::ILogger
    {
    public:
        void output_debug_string(std::string_view msg) override;
    };
}
