#include "Engine/Engine.h"

int main(void)
{
    // ���� �ν��Ͻ� ���� �� �ʱ�ȭ
    Engine engine;
    engine.Initialize("Project P");

    // ������ ���� ���� ����
    engine.GameLoop();

    // ���� ���� ó��
    engine.Shutdown();

    return 0;
}