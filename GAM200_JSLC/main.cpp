//main.cpp

#include "Engine/Engine.hpp"
#include "Engine/Logger.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#endif

int main(void)
{
#ifdef _WIN32
    // Must run before any delayed-import DLL is touched (glew/glfw/SDL/fmod use /DELAYLOAD on MSVC).
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0)
    {
        std::filesystem::path binDir = std::filesystem::path(exePath).parent_path() / L"bin";
        std::error_code ec;
        if (std::filesystem::is_directory(binDir, ec))
            SetDllDirectoryW(binDir.c_str());
    }
#endif
    // Initialize the logger before anything else
    // Set to output logs of Debug severity or higher to the console
    Logger::Instance().Initialize(Logger::Severity::Debug, true);

    Engine engine;
    if (!engine.Initialize("Project P"))
    {
        // Log an error message and terminate if engine initialization fails
        Logger::Instance().Log(Logger::Severity::Error, "Engine initialization failed!");
        return -1;
    }

    engine.GameLoop();
    engine.Shutdown();

    return 0;
}