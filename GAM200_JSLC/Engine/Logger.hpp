#pragma once
#include <string>
#include <fstream>
#include <chrono>

class Logger
{
public:
    // Logger�� ������ �ν��Ͻ��� �������� �Լ�
    static Logger& Instance();

    // �α��� �ɰ��� ������ ����
    enum class Severity
    {
        Verbose, // ���� ����� �޽���
        Debug,   // ����� �߿��� ����ϴ� �޽���
        Info,    // �Ϲ����� ���� �޽���
        Event,   // Ű �Է�, ���� ���� �� �Ϲ� �̺�Ʈ
        Error    // ���� �ε� ���� �� ���� �޽���
    };

    // �ΰ� �ʱ� ���� �Լ�
    void Initialize(Severity min_severity, bool use_console);

    // printf ��Ÿ���� ���� �ִ� �α��� ���� �Լ�
    void Log(Severity severity, const char* format, ...);

    // ���� �� ������ �����Ͽ� �̱��� ������ ����
    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;

private:
    // �����ڸ� private���� ����� �ܺ� ������ ����
    Logger();

    // ��� ����
    Severity min_severity;
    bool use_console_output;
    std::chrono::steady_clock::time_point start_time_point;
    std::ofstream log_file;

    // Helper �Լ�: ���� �α� �޽����� ��������
    std::string FormatMessage(const std::string& severity_text, const std::string& message);
};