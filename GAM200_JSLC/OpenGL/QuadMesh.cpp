#include "QuadMesh.hpp"

namespace OpenGL
{
    unsigned int QuadMesh::s_quadVAO = 0;
    unsigned int QuadMesh::s_quadVBO = 0;
    bool QuadMesh::s_initialized = false;

    void QuadMesh::Initialize()
    {
        if (s_initialized) return;

        // Standard quad vertices for sprite rendering (position + UV)
        float vertices[] = {
            -0.5f,  0.5f,   0.0f, 1.0f,  // Top-left
             0.5f, -0.5f,   1.0f, 0.0f,  // Bottom-right
            -0.5f, -0.5f,   0.0f, 0.0f,  // Bottom-left
            -0.5f,  0.5f,   0.0f, 1.0f,  // Top-left
             0.5f,  0.5f,   1.0f, 1.0f,  // Top-right
             0.5f, -0.5f,   1.0f, 0.0f   // Bottom-right
        };

        GL::GenVertexArrays(1, &s_quadVAO);
        GL::GenBuffers(1, &s_quadVBO);
        GL::BindVertexArray(s_quadVAO);
        GL::BindBuffer(0x8892, s_quadVBO); // GL_ARRAY_BUFFER = 0x8892
        GL::BufferData(0x8892, sizeof(vertices), vertices, 0x88E4); // GL_STATIC_DRAW = 0x88E4

        GL::VertexAttribPointer(0, 2, 0x1406, 0, 4 * sizeof(float), (void*)0); // GL_FLOAT = 0x1406, GL_FALSE = 0
        GL::EnableVertexAttribArray(0);
        GL::VertexAttribPointer(1, 2, 0x1406, 0, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        GL::EnableVertexAttribArray(1);

        GL::BindVertexArray(0);
        s_initialized = true;
    }

    void QuadMesh::Shutdown()
    {
        if (!s_initialized) return;

        GL::DeleteVertexArrays(1, &s_quadVAO);
        GL::DeleteBuffers(1, &s_quadVBO);
        s_quadVAO = 0;
        s_quadVBO = 0;
        s_initialized = false;
    }
}
