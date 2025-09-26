#include "Engine/Engine.hpp"
#include "Engine/Logger.hpp" // Logger ����� ���� ����

int main(void)
{
    // �ΰŸ� ���� ���� �ʱ�ȭ�մϴ�.
    // Debug ���� �̻��� �α׸� �ֿܼ� ����ϵ��� �����մϴ�.
    Logger::Instance().Initialize(Logger::Severity::Debug, true);

    Engine engine;
    if (!engine.Initialize("Project P"))
    {
        // ���� �ʱ�ȭ ���� �� ���� �α׸� ����� �����մϴ�.
        Logger::Instance().Log(Logger::Severity::Error, "Engine initialization failed!");
        return -1;
    }

    engine.GameLoop();
    engine.Shutdown();

    return 0;
}