#include "RenderingAPI.hpp"
#include "RGBA.hpp"
#include <GL/glew.h> // ���� OpenGL �Լ��� ����ϱ� ���� �����մϴ�.

namespace CS200
{
    void RenderingAPI::Init()
    {
        // 2D �������� ���� ����(���� ȥ��) ����� Ȱ��ȭ�մϴ�.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void RenderingAPI::SetClearColor(RGBA color)
    {
        // OpenGL�� ���� ���� �Լ��� ȣ���մϴ�.
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderingAPI::Clear()
    {
        // OpenGL�� ȭ�� ����� �Լ��� ȣ���մϴ�.
        // ������ ���� ���۸� ����ϴ�.
        glClear(GL_COLOR_BUFFER_BIT);
    }
}