#include "RenderingAPI.hpp"
#include "RGBA.hpp"
#include <GL/glew.h> 

namespace CS200
{
    void RenderingAPI::Init()
    {
        // 2D 렌더링을 위해 블렌딩(투명도 혼합) 기능을 활성화
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void RenderingAPI::SetClearColor(RGBA color)
    {
        // OpenGL의 배경색 설정 함수를 호출
        glClearColor(color.r, color.g, color.b, color.a);
    }

    void RenderingAPI::Clear()
    {
        // OpenGL의 화면 지우기 함수를 호출
        // 지금은 색상 버퍼만 지우도록 하자
        glClear(GL_COLOR_BUFFER_BIT);
    }
}