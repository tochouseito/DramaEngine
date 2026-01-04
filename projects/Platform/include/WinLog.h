#pragma once
#include "Core/include/ILog.h"

namespace Drama::Platform
{
    class WinLog final : public Drama::Core::IO::ILog
    {
    public:
        void output_debug_string(std::string_view msg) override;
    };
}
