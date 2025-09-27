#include "RenderingAPI.hpp"
#include "RGBA.hpp"
#include <GL/glew.h> 

namespace CS200
{
    void RenderingAPI::Init()
    {
        // 2D �������� ���� ����(���� ȥ��) ����� Ȱ��ȭ
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void RenderingAPI::SetClearColor(RGBA color)
    {
        // OpenGL�� ���� ���� �Լ��� ȣ��
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderingAPI::Clear()
    {
        // OpenGL�� ȭ�� ����� �Լ��� ȣ��
        // ������ ���� ���۸� ���쵵�� ����
        glClear(GL_COLOR_BUFFER_BIT);
    }
}