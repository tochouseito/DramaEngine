#pragma once
#include <string>
#include <string_view>

namespace Drama::Core::IO
{
    struct ILogger
    {
        virtual ~ILogger() = default;

        virtual void output_debug_string(std::string_view msg) = 0;
        virtual void message_box(std::string_view msg, std::string_view title = "Error") = 0;
    };
}
