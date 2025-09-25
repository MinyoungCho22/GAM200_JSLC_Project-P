#include "Engine/Engine.h"

int main(void)
{
    // 엔진 인스턴스 생성 및 초기화
    Engine engine;
    engine.Initialize("Project P");

    // 엔진의 메인 루프 실행
    engine.GameLoop();

    // 엔진 종료 처리
    engine.Shutdown();

    return 0;
}