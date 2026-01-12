//PulseGauge.cpp

#include "PulseGauge.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/QuadMesh.hpp"

void PulseGauge::Initialize(Math::Vec2 position, Math::Vec2 size)
{
    m_position = position;
    m_size = size;
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
    // Background bar
    Math::Matrix background_model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", background_model);
    shader.setVec3("objectColor", 0.2f, 0.2f, 0.2f);
    OpenGL::QuadMesh::Bind();
    OpenGL::QuadMesh::Draw();

    // Pulse bar (filled portion)
    Math::Vec2 pulse_bar_size = { m_size.x, m_size.y * m_pulse_ratio };
    Math::Vec2 pulse_bar_position = { m_position.x, m_position.y - (m_size.y - pulse_bar_size.y) / 2.0f };

    Math::Matrix pulse_model = Math::Matrix::CreateTranslation(pulse_bar_position) * Math::Matrix::CreateScale(pulse_bar_size);
    shader.setMat4("model", pulse_model);
    shader.setVec3("objectColor", 0.8f, 0.2f, 1.0f);
    OpenGL::QuadMesh::Draw();
    OpenGL::QuadMesh::Unbind();
}

void PulseGauge::Shutdown()
{
}
