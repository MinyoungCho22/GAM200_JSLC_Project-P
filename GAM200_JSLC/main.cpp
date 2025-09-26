#include "Engine/Engine.hpp"
#include "Engine/Logger.hpp" // Logger 사용을 위해 포함

int main(void)
{
    // 로거를 가장 먼저 초기화합니다.
    // Debug 레벨 이상의 로그를 콘솔에 출력하도록 설정합니다.
    Logger::Instance().Initialize(Logger::Severity::Debug, true);

    Engine engine;
    if (!engine.Initialize("Project P"))
    {
        // 엔진 초기화 실패 시 에러 로그를 남기고 종료합니다.
        Logger::Instance().Log(Logger::Severity::Error, "Engine initialization failed!");
        return -1;
    }

    engine.GameLoop();
    engine.Shutdown();

    return 0;
}