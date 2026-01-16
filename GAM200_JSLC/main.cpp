//main.cpp

#include "Engine/Engine.hpp"
#include "Engine/Logger.hpp" 

int main(void)
{
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