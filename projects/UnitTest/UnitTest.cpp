#include <iostream>

#include "Platform/public/Platform.h"
#include "Core/public/LogAssert.h"

int main()
{
    Drama::Platform::System platform;
    bool success = platform.init();
    if (!success)
    {
        return -1;
    }

    std::string logPath = platform.fs()->current_path() + "/temp/log.txt";
    Drama::Core::LogAssert::init(*platform.fs(), *platform.logger(), logPath);

    Drama::Core::LogAssert::log("This is a test log entry.");
    Drama::Core::LogAssert::assert(false, "This is a test assertion failure.");

    platform.shutdown();
    return 0;
}
