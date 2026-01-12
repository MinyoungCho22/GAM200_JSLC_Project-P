//PulseSource.cpp

#include "PulseSource.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/QuadMesh.hpp"
#include <algorithm>

void PulseSource::Initialize(Math::Vec2 position, Math::Vec2 size, float initial_pulse)
{
    m_position = position;
    m_size = size;
    m_max_pulse = initial_pulse;
    m_current_pulse = initial_pulse;
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

    float brightness = m_current_pulse / m_max_pulse;
    shader.setVec3("objectColor", 1.0f * brightness, 0.8f * brightness, 0.2f * brightness);

    OpenGL::QuadMesh::Bind();
    OpenGL::QuadMesh::Draw();
    OpenGL::QuadMesh::Unbind();
}

void PulseSource::Shutdown()
{
}