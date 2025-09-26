#include "RenderingAPI.hpp"
#include "RGBA.hpp"
#include <GL/glew.h> // 실제 OpenGL 함수를 사용하기 위해 포함합니다.

namespace CS200
{
    void RenderingAPI::Init()
    {
        // 2D 렌더링을 위해 블렌딩(투명도 혼합) 기능을 활성화합니다.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void RenderingAPI::SetClearColor(RGBA color)
    {
        // OpenGL의 배경색 설정 함수를 호출합니다.
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderingAPI::Clear()
    {
        // OpenGL의 화면 지우기 함수를 호출합니다.
        // 지금은 색상 버퍼만 지웁니다.
        glClear(GL_COLOR_BUFFER_BIT);
    }
}