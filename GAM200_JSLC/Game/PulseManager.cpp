#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PulseCore.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp"
#include "../Engine/Collision.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <random> 

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

constexpr float PI = 3.14159265359f;

std::default_random_engine generator;
std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

void PulseManager::Initialize()
{
    const char* texturePath = "Asset/Pulse.png";
    int totalFrames = 7;
    m_pulseAnim.frameDuration = 0.05f;

    GL::GenTextures(1, &m_pulseAnim.textureID);
    GL::BindTexture(GL_TEXTURE_2D, m_pulseAnim.textureID);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);
    if (data)
    {
        m_pulseAnim.texWidth = width;
        m_pulseAnim.texHeight = height;
        m_pulseAnim.frameWidth = width / totalFrames;
        m_pulseAnim.totalFrames = totalFrames;

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        Logger::Instance().Log(Logger::Severity::Info, "Pulse.png loaded for VFX.");
    }
    else
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to load Pulse.png!");
        stbi_image_free(data);
    }

    float vertices[] = {
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &m_pulseVAO);
    GL::GenBuffers(1, &m_pulseVBO);
    GL::BindVertexArray(m_pulseVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_pulseVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);
}

void PulseManager::Shutdown()
{
    if (m_pulseAnim.textureID != 0)
    {
        GL::DeleteTextures(1, &m_pulseAnim.textureID);
    }
    GL::DeleteVertexArrays(1, &m_pulseVAO);
    GL::DeleteBuffers(1, &m_pulseVBO);
}

void PulseManager::UpdateAttackVFX(bool isAttacking, Math::Vec2 startPos, Math::Vec2 endPos)
{
    m_isAttacking = isAttacking;
    if (m_isAttacking)
    {
        m_attackStartPos = startPos;
        m_attackEndPos = endPos;
    }
}

void PulseManager::Update(Math::Vec2 playerHitboxCenter, Math::Vec2 playerHitboxSize,
    Player& player, std::vector<PulseSource>& roomSources,
    std::vector<PulseSource>& hallwaySources,
    bool is_interact_key_pressed, double dt)
{
    m_vfxTimer += static_cast<float>(dt);

    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;

    auto checkSource = [&](PulseSource& source) {
        if (!source.HasPulse()) return;
        if (Collision::CheckAABB(playerHitboxCenter, playerHitboxSize, source.GetPosition(), source.GetSize()))
        {
            float dist_sq = (playerHitboxCenter - source.GetPosition()).LengthSq();
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
        };

    for (auto& source : roomSources)
    {
        checkSource(source);
    }
    for (auto& source : hallwaySources)
    {
        checkSource(source);
    }

    bool is_near_charger = (closest_source != nullptr);
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_charger, false, dt);

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);
        Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", result.delta);

        m_isCharging = true;
        m_chargeStartPos = closest_source->GetPosition();
        m_chargeEndPos = playerHitboxCenter;
    }
    else
    {
        m_isCharging = false;
    }
}

void PulseManager::DrawVFX(const Shader& shader) const
{
    if (!m_isAttacking && !m_isCharging) return;

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.use();
    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_pulseAnim.textureID);
    GL::BindVertexArray(m_pulseVAO);
    shader.setBool("flipX", false);

    const int NUM_FRAMES = m_pulseAnim.totalFrames;
    if (NUM_FRAMES == 0) return;

    float frame_w = static_cast<float>(m_pulseAnim.frameWidth);
    float frame_h = static_cast<float>(m_pulseAnim.texHeight);
    float aspect = frame_h / frame_w;
    Math::Vec2 baseParticleSize = { 32.f, 32.f * aspect };


    if (m_isAttacking)
    {
        Math::Vec2 vector = m_attackEndPos - m_attackStartPos;
        float totalLength = vector.Length();
        if (totalLength < 1.0f) return;

        Math::Vec2 dir = vector / totalLength;
        Math::Vec2 perp = dir.Perpendicular();

        float waveFrequency = 2.5f;
        float waveAmplitude = 15.0f;
        float waveSpeed = 2.0f;

        float initialYOffset1 = 10.0f;
        float initialYOffset2 = -10.0f;

        for (int line = 0; line < 2; ++line)
        {
            float currentInitialYOffset = (line == 0) ? initialYOffset1 : initialYOffset2;

            for (int i = 0; i < NUM_FRAMES; ++i)
            {
                float particleProgress = (float)(i) / (NUM_FRAMES - 1);
                float x_dist = totalLength * particleProgress;

                float wavePhase = fmod(m_vfxTimer * waveSpeed, 1.0f);
                float x_for_sine_normalized = particleProgress - wavePhase;
                if (x_for_sine_normalized < 0.0f) x_for_sine_normalized += 1.0f;

                float y_offset = sin(x_for_sine_normalized * waveFrequency * PI * 2.f) * waveAmplitude;

                Math::Vec2 pos = m_attackStartPos + (dir * x_dist) + (perp * (y_offset + currentInitialYOffset));
                Math::Vec2 particleSize = baseParticleSize;

                int frame = i;

                float rect_x = static_cast<float>(frame * m_pulseAnim.frameWidth) / m_pulseAnim.texWidth;
                float rect_w = static_cast<float>(m_pulseAnim.frameWidth) / m_pulseAnim.texWidth;
                shader.setVec4("spriteRect", rect_x, 0.0f, rect_w, 1.0f);

                Math::Matrix model = Math::Matrix::CreateTranslation(pos) * Math::Matrix::CreateScale(particleSize);
                shader.setMat4("model", model);
                GL::DrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }
    else if (m_isCharging)
    {
        Math::Vec2 vector = m_chargeEndPos - m_chargeStartPos;
        float totalLength = vector.Length();
        if (totalLength < 1.0f) return;

        Math::Vec2 dir = vector / totalLength;
        Math::Vec2 perp = dir.Perpendicular();

        float waveFrequency = 2.5f;
        float waveAmplitude = 15.0f;
        float waveSpeed = 2.0f;

        float initialYOffset1 = 10.0f;
        float initialYOffset2 = -10.0f;

        for (int line = 0; line < 2; ++line)
        {
            float currentInitialYOffset = (line == 0) ? initialYOffset1 : initialYOffset2;

            for (int i = 0; i < NUM_FRAMES; ++i)
            {
                float particleProgress = (float)(i) / (NUM_FRAMES - 1);
                float x_dist = totalLength * particleProgress;

                float wavePhase = fmod(m_vfxTimer * waveSpeed, 1.0f);
                float x_for_sine_normalized = particleProgress - wavePhase;
                if (x_for_sine_normalized < 0.0f) x_for_sine_normalized += 1.0f;

                float y_offset = sin(x_for_sine_normalized * waveFrequency * PI * 2.f) * waveAmplitude;

                Math::Vec2 pos = m_chargeStartPos + (dir * x_dist) + (perp * (y_offset + currentInitialYOffset));
                Math::Vec2 particleSize = baseParticleSize;

                int frame = (NUM_FRAMES - 1) - i;

                float rect_x = static_cast<float>(frame * m_pulseAnim.frameWidth) / m_pulseAnim.texWidth;
                float rect_w = static_cast<float>(m_pulseAnim.frameWidth) / m_pulseAnim.texWidth;
                shader.setVec4("spriteRect", rect_x, 0.0f, rect_w, 1.0f);

                Math::Matrix model = Math::Matrix::CreateTranslation(pos) * Math::Matrix::CreateScale(particleSize);
                shader.setMat4("model", model);
                GL::DrawArrays(GL_TRIANGLES, 0, 6);
            }
        }
    }

    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    GL::BindVertexArray(0);
    GL::Disable(GL_BLEND);
}