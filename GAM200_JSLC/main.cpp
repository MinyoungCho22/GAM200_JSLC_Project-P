#include "Engine/Engine.hpp"
#include "Engine/Logger.hpp" 

int main(void)
{
    // �ΰŸ� ���� ���� �ʱ�ȭ
    // Debug ���� �̻��� �α׸� �ֿܼ� ����ϵ��� ����
    Logger::Instance().Initialize(Logger::Severity::Debug, true);

    Engine engine;
    if (!engine.Initialize("Project P"))
    {
        // ���� �ʱ�ȭ ���� �� ���� �α׸� ����� ����
        Logger::Instance().Log(Logger::Severity::Error, "Engine initialization failed!");
        return -1;
    }

    engine.GameLoop();
    engine.Shutdown();

    return 0;
}