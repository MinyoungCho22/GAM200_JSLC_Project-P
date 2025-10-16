#include "DebugRenderer.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <glad/glad.h>
#include <vector>
#include <cmath>

void DebugRenderer::Initialize()
{
    // --- Circle VAO ---
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    std::vector<float> vertices;
    float angle_step = 2.0f * 3.1415926535f / CIRCLE_SEGMENTS;
    for (int i = 0; i <= CIRCLE_SEGMENTS; ++i)
    {
        vertices.push_back(std::cos(angle_step * i));
        vertices.push_back(std::sin(angle_step * i));
    }
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // --- Box VAO ---
    float box_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };
    unsigned int box_indices[] = { 0, 1, 1, 2, 2, 3, 3, 0 };
    glGenVertexArrays(1, &boxVAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(boxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(box_vertices), box_vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(box_indices), box_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void DebugRenderer::Shutdown()
{
    glDeleteVertexArrays(1, &circleVAO);
    glDeleteVertexArrays(1, &boxVAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void DebugRenderer::DrawCircle(Shader& shader, Math::Vec2 center, float radius, Math::Vec2 color)
{
    Math::Matrix model = Math::Matrix::CreateTranslation(center) * Math::Matrix::CreateScale({ radius, radius });
    shader.setMat4("model", model);
    shader.setVec3("objectColor", color.x, color.y, 0.8f); // 파란색 계열

    glBindVertexArray(circleVAO);
    glDrawArrays(GL_LINE_LOOP, 0, CIRCLE_SEGMENTS + 1);
    glBindVertexArray(0);
}

void DebugRenderer::DrawBox(Shader& shader, Math::Vec2 pos, Math::Vec2 size, Math::Vec2 color)
{
    Math::Matrix model = Math::Matrix::CreateTranslation(pos) * Math::Matrix::CreateScale(size);
    shader.setMat4("model", model);
    shader.setVec3("objectColor", color.x, color.y, 0.2f); // 초록색 계열

    glBindVertexArray(boxVAO);
    glDrawElements(GL_LINES, 8, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}