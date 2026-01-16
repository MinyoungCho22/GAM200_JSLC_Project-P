//Logger.hpp

#pragma once
#include <string>
#include <fstream>
#include <chrono>

class Logger
{
public:
    // Get singleton instance
    static Logger& Instance();

    // Log severity levels
    enum class Severity
    {
        Verbose,
        Debug,
        Info,
        Event,
        Error
    };

    // Initialize logger (Must be called first)
    void Initialize(Severity min_severity, bool use_console);

    // Log message
    void Log(Severity severity, const char* format, ...);

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    Severity min_severity;
    bool use_console_output;
    std::chrono::steady_clock::time_point start_time_point;
    std::ofstream log_file;

    // Helper function to format message
    std::string FormatMessage(const std::string& severity_text, const std::string& message);
};