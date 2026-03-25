//PulseSource.cpp

#include "PulseSource.hpp"
#include "Background.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <algorithm>

void PulseSource::Initialize(Math::Vec2 position, Math::Vec2 size, float initial_pulse)
{
    m_position = position;
    m_size = size;
    m_max_pulse = initial_pulse;
    m_current_pulse = initial_pulse;

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

    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void PulseSource::InitializeSprite(const char* texPath, Math::Vec2 pivot)
{
    m_pivot  = pivot;
    m_sprite = std::make_unique<Background>();
    m_sprite->Initialize(texPath);
}

void PulseSource::DrawSprite(Shader& textureShader) const
{
    if (!m_sprite) return;

    // Pivot offset: pivot=(0.5,0.5) keeps m_position as render center.
    // pivot=(0,1) treats m_position as top-left and shifts center down-right.
    Math::Vec2 renderPos = {
        m_position.x + (0.5f - m_pivot.x) * m_size.x,
        m_position.y + (0.5f - m_pivot.y) * m_size.y
    };

    textureShader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
    textureShader.setBool("flipX", false);

    Math::Matrix model = Math::Matrix::CreateTranslation(renderPos) * Math::Matrix::CreateScale(m_size);
    m_sprite->Draw(textureShader, model);
}

void PulseSource::DrawOutline(Shader& outlineShader) const
{
    if (!m_sprite) return;

    int w = m_sprite->GetWidth();
    int h = m_sprite->GetHeight();
    if (w <= 0 || h <= 0) return;

    outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
    outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
    outlineShader.setFloat("outlineWidthTexels", 2.0f);

    Math::Vec2 renderPos = {
        m_position.x + (0.5f - m_pivot.x) * m_size.x,
        m_position.y + (0.5f - m_pivot.y) * m_size.y
    };
    Math::Matrix model = Math::Matrix::CreateTranslation(renderPos) * Math::Matrix::CreateScale(m_size);
    m_sprite->Draw(outlineShader, model);
}

void PulseSource::Shutdown()
{
    if (m_sprite)
    {
        m_sprite->Shutdown();
        m_sprite.reset();
    }
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
}