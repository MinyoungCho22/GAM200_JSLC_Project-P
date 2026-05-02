//Logger.cpp

#include "Logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
// Windows.h 매크로 FormatMessage가 Logger::FormatMessage 멤버와 충돌
#ifdef FormatMessage
#undef FormatMessage
#endif
#endif

Logger& Logger::Instance()
{
    static Logger instance;
    return instance;
}

Logger::Logger() 
    : min_severity(Severity::Debug), use_console_output(true)
{
    start_time_point = std::chrono::steady_clock::now();
}

Logger::~Logger()
{
    if (log_file.is_open())
    {
        log_file.close();
    }
}

void Logger::Initialize(Severity severity, bool use_console)
{
    min_severity = severity;
    use_console_output = use_console;
#ifdef _WIN32
    // std::cout UTF-8과 터미널 코드 페이지 맞춤 — 깨진 글자·이상한 커서 동작 완화
    if (use_console)
    {
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        // 일부 환경에서 글자색이 배경과 같아져 드래그(선택)할 때만 보이는 문제 완화
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr && GetFileType(hOut) == FILE_TYPE_CHAR)
        {
            SetConsoleTextAttribute(
                hOut,
                FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }
    }
#endif
    // Disable file logging; keep console output only.
    if (log_file.is_open())
    {
        log_file.close();
    }

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