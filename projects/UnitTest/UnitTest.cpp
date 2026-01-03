#include <iostream>

#include "Platform/include/WinFileSystem.h"
#include "Core/include/LogAssert.h"

class EngineContext
{
public:
    void SetFileSystem(Drama::Core::IO::IFileSystem& fs) { m_Fs = &fs; }
    Drama::Core::IO::IFileSystem& Fs() { return *m_Fs; }

private:
    Drama::Core::IO::IFileSystem* m_Fs = nullptr;
};

int main()
{
    Drama::Platform::IO::WinFileSystem winFs;

    EngineContext ctx;
    ctx.SetFileSystem(winFs);

    std::string logPath = ctx.Fs().current_path() + "/temp/log.txt";

    Drama::Core::LogAssert::init(ctx.Fs(), logPath);
    Drama::Core::LogAssert::write_line("This is a test log entry.");
}
