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
    m_sharedCurrent = std::make_shared<float>(initial_pulse);
    m_sharedMax = std::make_shared<float>(initial_pulse);
    m_sharedGaugeUnlocked = std::make_shared<bool>(false);

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

void PulseSource::SharePulseStorageWith(PulseSource& leader)
{
    m_sharedCurrent = leader.m_sharedCurrent;
    m_sharedMax = leader.m_sharedMax;
    m_sharedGaugeUnlocked = leader.m_sharedGaugeUnlocked;
}

float PulseSource::Drain(float amount)
{
    if (!m_sharedCurrent) return 0.0f;
    float drained_amount = std::min(amount, *m_sharedCurrent);
    *m_sharedCurrent -= drained_amount;
    if (drained_amount > 0.0f)
        *m_sharedGaugeUnlocked = true;
    if (*m_sharedCurrent < 0.0f)
        *m_sharedCurrent = 0.0f;
    return drained_amount;
}

void PulseSource::RefillStock()
{
    if (!m_sharedCurrent || !m_sharedMax) return;
    *m_sharedCurrent = *m_sharedMax;
    if (m_sharedGaugeUnlocked)
        *m_sharedGaugeUnlocked = false;
}

void PulseSource::Draw(Shader& shader) const
{
    Math::Matrix scale = Math::Matrix::CreateScale(m_size);
    Math::Matrix translation = Math::Matrix::CreateTranslation(m_position);
    Math::Matrix model = translation * scale;

    shader.setMat4("model", model);

    float brightness = (*m_sharedMax > 0.0f) ? (*m_sharedCurrent / *m_sharedMax) : 0.0f;
    shader.setVec3("objectColor", 1.0f * brightness, 0.8f * brightness, 0.2f * brightness);

    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void PulseSource::InitializeSprite(const char* texPath, Math::Vec2 pivot)
{
    m_pivot = pivot;
    m_sprite = std::make_unique<Background>();
    m_sprite->Initialize(texPath);
}

void PulseSource::DrawSprite(Shader& textureShader) const
{
    if (!m_sprite) return;

    Math::Vec2 renderPos = {
        m_position.x + (0.5f - m_pivot.x) * m_size.x,
        m_position.y + (0.5f - m_pivot.y) * m_size.y
    };

    textureShader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
    textureShader.setBool("flipX", false);

    Math::Matrix model = Math::Matrix::CreateTranslation(renderPos) * Math::Matrix::CreateScale(m_size);
    m_sprite->Draw(textureShader, model);
}

void PulseSource::DrawRemainGauge(Shader& colorShader) const
{
    if (!m_drawRemainGauge || !m_sharedGaugeUnlocked || !m_sharedMax || VAO == 0)
        return;
    if (!*m_sharedGaugeUnlocked || *m_sharedMax <= 0.0f)
        return;

    float ratio = *m_sharedCurrent / *m_sharedMax;
    ratio = std::max(0.0f, std::min(1.0f, ratio));

    const float barW = 10.0f;
    const float barH = std::min(110.0f, std::max(44.0f, m_size.y * 0.9f));
    const Math::Vec2 half = m_size * 0.5f;
    const float padY = 10.0f;
    const float horizNudgeRight = 38.0f;
    const float vertNudgeDown = -8.0f;

    const float rx = m_position.x + half.x;
    const float ty = m_position.y + half.y;
    const float barCenterX = rx - 10.0f - barW * 0.5f + horizNudgeRight;
    const float innerTopY = ty + padY + vertNudgeDown;
    const float innerCenterY = innerTopY - barH * 0.5f;

    Math::Vec2 bgSize = { barW + 6.0f, barH + 6.0f };
    Math::Matrix bgModel = Math::Matrix::CreateTranslation({ barCenterX, innerCenterY }) * Math::Matrix::CreateScale(bgSize);
    colorShader.setMat4("model", bgModel);
    colorShader.setVec3("objectColor", 0.2f, 0.2f, 0.2f);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    float fillHeight = barH * ratio;
    if (fillHeight > 0.5f)
    {
        const float innerBottomY = innerTopY - barH;
        const float fillCenterY = innerBottomY + fillHeight * 0.5f;
        Math::Matrix fgModel = Math::Matrix::CreateTranslation({ barCenterX, fillCenterY }) * Math::Matrix::CreateScale({ barW, fillHeight });
        colorShader.setMat4("model", fgModel);
        colorShader.setVec3("objectColor", 0.8f, 0.2f, 1.0f);
        GL::DrawArrays(GL_TRIANGLES, 0, 6);
    }
    GL::BindVertexArray(0);
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
    VAO = 0;
    VBO = 0;
    m_sharedCurrent.reset();
    m_sharedMax.reset();
    m_sharedGaugeUnlocked.reset();
}
