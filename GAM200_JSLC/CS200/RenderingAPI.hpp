#pragma once

namespace CS200
{
    struct RGBA; // RGBA ����ü�� ����Ѵٰ� �̸� �˷��ݴϴ� (���� ����).

    class RenderingAPI
    {
    public:
        // ������ API�� �ʱ�ȭ�մϴ�.
        static void Init();

        // ȭ���� ���� ������ �����մϴ�.
        static void SetClearColor(RGBA color);

        // ������ �������� ȭ���� ����ϴ�.
        static void Clear();
    };
}