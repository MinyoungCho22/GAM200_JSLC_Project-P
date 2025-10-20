#include "PulseGauge.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"

void PulseGauge::Initialize(Math::Vec2 position, Math::Vec2 size)
{
    m_position = position;
    m_size = size;

    float vertices[] = {
        -0.5f,  0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f
    };

    GL::GenVertexArrays(1, &VAO);
    GL::GenBuffers(1, &VBO);
    GL::BindVertexArray(VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);
}

void PulseGauge::Update(float current_pulse, float max_pulse)
{
    if (max_pulse > 0)
    {
        m_pulse_ratio = current_pulse / max_pulse;
    }
    else
    {
        m_pulse_ratio = 0.0f;
    }
}

void PulseGauge::Draw(Shader& shader)
{
    // 1. 배경 바 그리기 (어두운 회색)
    Math::Matrix background_model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", background_model);
    shader.setVec3("objectColor", 0.2f, 0.2f, 0.2f);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    // 2. 펄스 바 그리기 (밝은 보라색)
    Math::Vec2 pulse_bar_size = { m_size.x, m_size.y * m_pulse_ratio };
    Math::Vec2 pulse_bar_position = { m_position.x, m_position.y - (m_size.y - pulse_bar_size.y) / 2.0f };

    Math::Matrix pulse_model = Math::Matrix::CreateTranslation(pulse_bar_position) * Math::Matrix::CreateScale(pulse_bar_size);
    shader.setMat4("model", pulse_model);
    shader.setVec3("objectColor", 0.8f, 0.2f, 1.0f);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    GL::BindVertexArray(0);
}

void PulseGauge::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
}