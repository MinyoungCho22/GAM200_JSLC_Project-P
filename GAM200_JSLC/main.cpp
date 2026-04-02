//main.cpp

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <filesystem>
#endif
#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#include <filesystem>
#endif

#include "Engine/Engine.hpp"
#include "Engine/Logger.hpp"

int main(void)
{
#if defined(__linux__)
    // Match Windows behaviour: Asset/, Config/, OpenGL/Shaders/ resolve from the executable directory.
    char exePath[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (n > 0)
    {
        exePath[n] = '\0';
        std::error_code ec;
        std::filesystem::current_path(std::filesystem::path(exePath).parent_path(), ec);
    }
#endif
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