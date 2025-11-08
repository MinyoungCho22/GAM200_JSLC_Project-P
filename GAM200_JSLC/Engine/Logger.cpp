#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdarg>


Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}


Logger::Logger() : min_severity(Severity::Debug), use_console_output(true)
{
    start_time_point = std::chrono::steady_clock::now();
}

void Logger::Initialize(Severity severity, bool use_console)
{
    min_severity = severity;
    use_console_output = use_console;
    log_file.open("GameLog.txt", std::ios::out | std::ios::trunc);
    Log(Severity::Info, "Logger initialized.");
}

void Logger::Log(Severity severity, const char* format, ...)
{
    if (severity < min_severity)
    {
        return;
    }

    std::string severity_text;
    switch (severity)
    {
    case Severity::Verbose: severity_text = "Verbose"; break;
    case Severity::Debug:   severity_text = "Debug";   break;
    case Severity::Info:    severity_text = "Info";    break;
    case Severity::Event:   severity_text = "Event";   break;
    case Severity::Error:   severity_text = "Error";   break;
    }

    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    std::string formatted_message = FormatMessage(severity_text, buffer);

    if (log_file.is_open())
    {
        log_file << formatted_message << std::endl;
        log_file.flush();
    }

    if (use_console_output)
    {
        std::cout << formatted_message << std::endl;
    }
}

std::string Logger::FormatMessage(const std::string& severity_text, const std::string& message)
{
    auto current_time = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time_point).count();

    std::stringstream ss;
    ss << "[" << std::setw(7) << elapsed_ms << "ms] ";
    ss << "[" << std::setw(7) << std::left << severity_text << "] ";
    ss << message;

    return ss.str();
}