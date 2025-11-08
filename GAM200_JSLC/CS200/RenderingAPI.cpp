#include "RenderingAPI.hpp"
#include "RGBA.hpp"
#include "../OpenGL/GLWrapper.hpp" 

namespace CS200
{
    void RenderingAPI::Init()
    {
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
    }

    void RenderingAPI::SetClearColor(RGBA color)
    {
        GL::ClearColor(color.r, color.g, color.b, color.a);  
    }

    void RenderingAPI::Clear()
    {
        GL::Clear(GL_COLOR_BUFFER_BIT); // 
    }
}