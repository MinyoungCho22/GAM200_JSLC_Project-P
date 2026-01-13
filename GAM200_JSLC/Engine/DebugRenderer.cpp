// DebugRenderer.cpp

#include "DebugRenderer.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <vector>
#include <cmath>

void DebugRenderer::Initialize()
{
    // --- Circle VAO ---
    GL::GenVertexArrays(1, &circleVAO);
    unsigned int circleVBO;
    GL::GenBuffers(1, &circleVBO);
    GL::BindVertexArray(circleVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, circleVBO);

    std::vector<float> vertices;
    float angle_step = 2.0f * 3.1415926535f / CIRCLE_SEGMENTS;
    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i)
    {
        vertices.push_back(std::cos(angle_step * i));
        vertices.push_back(std::sin(angle_step * i));
    }

    GL::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);

    // --- Box VAO ---
    float box_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
    unsigned int box_indices[] = { 0, 1, 1, 2, 2, 3, 3, 0 };

    GL::GenVertexArrays(1, &boxVAO);
    GL::GenBuffers(1, &VBO);
    GL::GenBuffers(1, &EBO);
    GL::BindVertexArray(boxVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);
    GL::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    GL::BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(box_indices), box_indices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);

    // --- Line VAO ---
    float line_vertices[] = {
        -0.5f, 0.0f,
         0.5f, 0.0f
    };

    GL::GenVertexArrays(1, &lineVAO);
    unsigned int lineVBO;
    GL::GenBuffers(1, &lineVBO);
    GL::BindVertexArray(lineVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, lineVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);

    GL::BindVertexArray(0);
}

void DebugRenderer::Shutdown() const
{
    GL::DeleteVertexArrays(1, &circleVAO);
    GL::DeleteVertexArrays(1, &boxVAO);
    GL::DeleteVertexArrays(1, &lineVAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteBuffers(1, &EBO);
}

void DebugRenderer::DrawCircle(Shader& shader, Math::Vec2 center, float radius, Math::Vec2 color) const
{
    Math::Matrix model = Math::Matrix::CreateTranslation(center) * Math::Matrix::CreateScale({ radius, radius });
    shader.setMat4("model", model);
    shader.setVec3("objectColor", color.x, color.y, 0.8f);

    GL::BindVertexArray(circleVAO);
    GL::DrawArrays(GL_LINE_LOOP, 0, CIRCLE_SEGMENTS + 1);
    GL::BindVertexArray(0);
}

void DebugRenderer::DrawBox(Shader& shader, Math::Vec2 pos, Math::Vec2 size, Math::Vec2 color) const
{
    Math::Matrix model = Math::Matrix::CreateTranslation(pos) * Math::Matrix::CreateScale(size);
    shader.setMat4("model", model);
    shader.setVec3("objectColor", color.x, color.y, 0.2f);

    GL::BindVertexArray(boxVAO);
    GL::DrawElements(GL_LINES, 8, GL_UNSIGNED_INT, 0);
    GL::BindVertexArray(0);
}

void DebugRenderer::DrawLine(const Shader& shader, Math::Vec2 start, Math::Vec2 end, float r, float g, float b) const
{
    Math::Vec2 direction = end - start;
    float length = direction.Length();
    Math::Vec2 center = (start + end) * 0.5f;

    float angle = std::atan2(direction.y, direction.x) * (180.0f / 3.14159265359f);

    Math::Matrix model = Math::Matrix::CreateTranslation(center) *
        Math::Matrix::CreateRotation(angle) *
        Math::Matrix::CreateScale({ length, 1.0f });

    shader.setMat4("model", model);
    shader.setVec3("objectColor", r, g, b);

    GL::BindVertexArray(lineVAO);
    GL::DrawArrays(GL_LINES, 0, 2);
    GL::BindVertexArray(0);
}