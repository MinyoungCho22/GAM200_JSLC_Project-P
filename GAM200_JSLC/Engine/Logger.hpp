#pragma once
#include <string>
#include <fstream>
#include <chrono>

class Logger
{
public:
    // Logger의 유일한 인스턴스를 가져오는 함수
    static Logger& Instance();

    // 로그의 심각도 수준을 정의합니다.
    enum class Severity
    {
        Verbose, // 아주 사소한 메시지
        Debug,   // 디버깅 중에만 사용하는 메시지
        Info,    // 일반적인 정보 메시지
        Event,   // 키 입력, 상태 변경 등 일반 이벤트
        Error    // 파일 로드 실패 등 오류 메시지
    };

    // 로거 초기 설정 함수
    void Initialize(Severity min_severity, bool use_console);

    // printf 스타일의 서식 있는 로깅을 위한 함수
    void Log(Severity severity, const char* format, ...);

    // 복사 및 대입을 방지하여 싱글턴 패턴을 유지합니다.
    Logger(const Logger&) = delete;
    void operator=(const Logger&) = delete;

private:
    // 생성자를 private으로 만들어 외부 생성을 막습니다.
    Logger();

    // 멤버 변수
    Severity min_severity;
    bool use_console_output;
    std::chrono::steady_clock::time_point start_time_point;
    std::ofstream log_file;

    // Helper 함수: 실제 로그 메시지를 포맷팅합니다.
    std::string FormatMessage(const std::string& severity_text, const std::string& message);
};