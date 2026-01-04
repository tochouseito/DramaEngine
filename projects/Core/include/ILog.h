#pragma once
#include <string>
#include <string_view>

namespace Drama::Core::IO
{
    struct ILog
    {
        virtual ~ILog() = default;

        virtual void output_debug_string(std::string_view msg) = 0;
    };
}
