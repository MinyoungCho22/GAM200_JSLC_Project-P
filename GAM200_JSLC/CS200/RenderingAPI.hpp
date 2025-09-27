#pragma once

namespace CS200
{
    struct RGBA; // RGBA ����ü�� ����Ѵٰ� �̸� �˷����� (���� ����).

    class RenderingAPI
    {
    public:
        // ������ API�� �ʱ�ȭ
        static void Init();

        // ȭ���� ���� ������ ��������
        static void SetClearColor(RGBA color);

        // ������ �������� ȭ���� ����
        static void Clear();
    };
}