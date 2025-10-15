#include "PulseSource.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <glad/glad.h>
#include <algorithm>

void PulseSource::Initialize(Math::Vec2 position, Math::Vec2 size, float initial_pulse)
{
    m_position = position;
    m_size = size;
    m_max_pulse = initial_pulse;
    m_current_pulse = initial_pulse;

    // 렌더링할 사각형의 정점 데이터 (중심 기준)
    float vertices[] = {
        -0.5f,  0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

float PulseSource::Drain(float amount)
{
    float drained_amount = std::min(amount, m_current_pulse);
    m_current_pulse -= drained_amount;
    if (m_current_pulse < 0)
    {
        m_current_pulse = 0;
    }
    return drained_amount;
}

void PulseSource::Draw(Shader& shader) const
{
    Math::Matrix scale = Math::Matrix::CreateScale(m_size);
    Math::Matrix translation = Math::Matrix::CreateTranslation(m_position);
    Math::Matrix model = translation * scale;

    shader.setMat4("model", model);

    // 남은 펄스 양에 따라 밝기(색상)를 조절
    float brightness = m_current_pulse / m_max_pulse;
    shader.setVec3("objectColor", 1.0f * brightness, 0.8f * brightness, 0.2f * brightness); // 밝은 노란색 계열

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void PulseSource::Shutdown()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}