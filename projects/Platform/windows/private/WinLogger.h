#pragma once
#include "Core/IO/public/ILogger.h"

namespace Drama::Platform::Win
{
    class WinLogger final : public Drama::Core::IO::ILogger
    {
    public:
        void output_debug_string([[maybe_unused]] std::string_view msg) override;
        void message_box(std::string_view msg, std::string_view title = "Error") override;
    };
}
