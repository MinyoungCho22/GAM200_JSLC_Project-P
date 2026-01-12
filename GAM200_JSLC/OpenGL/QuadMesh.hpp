#pragma once
#include "GLWrapper.hpp"

namespace OpenGL
{
    // Shared quad mesh for sprite rendering
    // All sprites use the same quad geometry, so we share one VAO/VBO
    class QuadMesh
    {
    public:
        static void Initialize();
        static void Shutdown();
        static unsigned int GetVAO() { return s_quadVAO; }
        static void Bind() { GL::BindVertexArray(s_quadVAO); }
        static void Unbind() { GL::BindVertexArray(0); }
        static void Draw() { GL::DrawArrays(0x0004, 0, 6); } // GL_TRIANGLES = 0x0004

    private:
        static unsigned int s_quadVAO;
        static unsigned int s_quadVBO;
        static bool s_initialized;
    };
}
