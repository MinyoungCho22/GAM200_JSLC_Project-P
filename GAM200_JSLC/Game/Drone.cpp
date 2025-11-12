#include "Drone.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <cmath>

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

constexpr float PI = 3.14159265359f;
constexpr float GROUND_LEVEL = 180.0f;

void Drone::Init(Math::Vec2 startPos, const char* texturePath)
{
    m_position = startPos;
    m_baseY = startPos.y;
    m_velocity = { 0.0f, 0.0f };
    m_direction = { 1.0f, 0.0f };

    float vertices[] = {
        -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f, -0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,     0.0f, 0.0f,
        -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f,  0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,     1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &VAO);
    GL::GenBuffers(1, &VBO);
    GL::BindVertexArray(VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);

    GL::GenTextures(1, &textureID);
    GL::BindTexture(GL_TEXTURE_2D, textureID);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);

        float desiredWidth = 120.0f;
        float aspectRatio = static_cast<float>(height) / static_cast<float>(width);
        m_size = { desiredWidth, desiredWidth * aspectRatio };

        Logger::Instance().Log(Logger::Severity::Info, "Drone texture loaded: %s", texturePath);
    }
    else
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to load drone texture: %s", texturePath);
    }

    stbi_image_free(data);
}

void Drone::Update(double dt, const Player& player, Math::Vec2 playerHitboxSize)
{
    if (m_isDead) return;

    if (!m_isHit)
    {
        m_radarAngle += m_radarRotationSpeed * static_cast<float>(dt);
        if (m_radarAngle >= 360.0f) m_radarAngle -= 360.0f;
    }

    if (m_isHit)
    {
        m_hitTimer -= static_cast<float>(dt);

        m_hitRotation += 720.0f * static_cast<float>(dt);
        if (m_hitRotation > 360.0f) m_hitRotation -= 360.0f;

        m_fallSpeed += 800.0f * static_cast<float>(dt);
        m_position.y -= m_fallSpeed * static_cast<float>(dt);

        if (m_position.y < GROUND_LEVEL + m_size.y / 2.0f)
        {
            m_position.y = GROUND_LEVEL + m_size.y / 2.0f;
            m_isDead = true;
            Logger::Instance().Log(Logger::Severity::Event, "Drone destroyed and landed on ground!");
        }
        return;
    }

    if (m_attackCooldown > 0.0f)
    {
        m_attackCooldown -= static_cast<float>(dt);
    }

    Math::Vec2 toPlayer = player.GetPosition() - m_position;
    float distSq = toPlayer.LengthSq();

    float playerHitboxRadius = (playerHitboxSize.x + playerHitboxSize.y) * 0.15f;
    float effectiveDetectionRange = DETECTION_RANGE + playerHitboxRadius;
    float effectiveDetectionRangeSq = effectiveDetectionRange * effectiveDetectionRange;

    if (m_isAttacking)
    {
        m_attackTimer += static_cast<float>(dt);

        float progress = m_attackTimer / m_attackDuration;
        if (progress >= 1.0f)
        {
            m_isAttacking = false;
            m_position = m_attackStartPos;
            m_attackTimer = 0.0f;
        }
        else
        {
            m_attackAngle += (360.0f / m_attackDuration) * static_cast<float>(dt) * m_attackDirection;

            float radians = m_attackAngle * (PI / 180.0f);
            m_position.x = m_attackCenter.x + std::cos(radians) * m_attackRadius;
            m_position.y = m_attackCenter.y + std::sin(radians) * m_attackRadius;
        }
    }
    else if (distSq < effectiveDetectionRangeSq && m_attackCooldown <= 0.0f)
    {
        m_isAttacking = true;
        m_shouldDealDamage = true;
        m_attackTimer = 0.0f;
        m_attackStartPos = m_position;
        m_attackCooldown = m_attackCooldownDuration;

        m_attackCenter = (m_position + player.GetPosition()) * 0.5f;

        Math::Vec2 toCenter = m_attackCenter - m_position;
        m_attackAngle = std::atan2(toCenter.y, toCenter.x) * (180.0f / PI);

        m_attackDirection = (player.GetPosition().x > m_position.x) ? 1 : -1;

        Logger::Instance().Log(Logger::Severity::Event, "Drone attacking! Distance: %.1f, Effective Range: %.1f",
            std::sqrt(distSq), effectiveDetectionRange);
    }
    else
    {
        m_moveTimer += static_cast<float>(dt);
        if (m_moveTimer > 3.0f)
        {
            m_moveTimer = 0.0f;
            m_direction.x = -m_direction.x;
        }

        m_bobTimer += static_cast<float>(dt);

        m_velocity = m_direction * m_speed;
        m_position.x += m_velocity.x * static_cast<float>(dt);

        const float bobAmplitude = 20.0f;
        const float bobFrequency = 3.0f;

        float bobOffset = std::sin(m_bobTimer * bobFrequency) * bobAmplitude;

        m_position.y = m_baseY + bobOffset;
    }
}

void Drone::Draw(const Shader& shader) const
{
    Math::Matrix rotationMatrix = Math::Matrix::CreateIdentity();

    if (m_isDead)
    {
        rotationMatrix = Math::Matrix::CreateRotation(m_hitRotation);
    }
    else if (m_isHit)
    {
        float wobble = std::sin(m_hitTimer * 20.0f) * 45.0f;
        rotationMatrix = Math::Matrix::CreateRotation(m_hitRotation + wobble);
    }
    else if (m_isAttacking)
    {
        float tiltAngle = m_attackAngle + 90.0f * m_attackDirection;
        rotationMatrix = Math::Matrix::CreateRotation(tiltAngle);
    }

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(m_size);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation(m_position);
    Math::Matrix model = transMatrix * rotationMatrix * scaleMatrix;

    shader.use();
    shader.setMat4("model", model);
    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", m_direction.x < 0.0f);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureID);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Drone::DrawRadar(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_isDead) return;

    const int numLines = 12;
    const float sweepAngle = 45.0f;
    float halfSweep = sweepAngle / 2.0f;

    for (int i = 0; i <= numLines; ++i)
    {
        float angle = (m_radarAngle - halfSweep + (sweepAngle * i / numLines)) * (PI / 180.0f);

        Math::Vec2 lineEnd = m_position;
        lineEnd.x += std::cos(angle) * m_radarLength;
        lineEnd.y += std::sin(angle) * m_radarLength;

        float brightness = 1.0f - (std::abs(i - numLines / 2.0f) / (numLines / 2.0f)) * 0.7f;
        debugRenderer.DrawLine(colorShader, m_position, lineEnd, brightness, 0.0f, 0.0f);
    }
}

void Drone::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteTextures(1, &textureID);
}

void Drone::TakeHit()
{
    if (m_isHit || m_isDead) return;

    m_isHit = true;
    m_hitTimer = m_hitDuration;
    m_hitRotation = 0.0f;
    m_fallSpeed = 0.0f;
    m_isAttacking = false;

    Logger::Instance().Log(Logger::Severity::Event, "Drone hit! Starting death sequence.");
}