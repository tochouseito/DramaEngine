#include <iostream>
#include <Windows.h>

#include "projects/Platform/Platform.h"

int main()
{
    using namespace Drama::Platform;
    Windows window;
    if (!window.Create(800, 600))
    {
        std::cerr << "Failed to create window." << std::endl;
        return -1;
    }
    window.Show();
    bool running = true;
    while (running)
    {
        running = window.PumpMessages();
        // Here you can add additional update logic if needed
    }
    window.Shutdown();
    return 0;
}
