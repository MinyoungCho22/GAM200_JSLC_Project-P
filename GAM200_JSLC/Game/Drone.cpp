//Drone.cpp

#include "Drone.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <cmath>
#include <random>

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

constexpr float PI = 3.14159265359f;
constexpr float GROUND_LEVEL = 180.0f;
constexpr float ROOFTOP_GROUND_LEVEL = 1460.0f;
constexpr float ROOFTOP_MIN_Y = 1080.0f;

std::default_random_engine drone_generator;
std::uniform_real_distribution<float> drone_distribution(-1.0f, 1.0f);
std::uniform_real_distribution<float> drone_angle_distribution(25.0f, 45.0f);

void Drone::Init(Math::Vec2 startPos, const char* texturePath, bool isTracer)
{
    m_position = startPos;
    m_baseY = startPos.y;
    m_velocity = { 0.0f, 0.0f };
    m_direction = { 1.0f, 0.0f };
    m_isTracer = isTracer;
    m_isChasing = false;
    m_lostTimer = 0.0f;
    m_currentSpeed = m_baseSpeed;

    if (startPos.y >= ROOFTOP_MIN_Y)
    {
        m_groundLevel = ROOFTOP_GROUND_LEVEL;
    }
    else
    {
        m_groundLevel = GROUND_LEVEL;
    }

    m_searchMaxAngle = drone_angle_distribution(drone_generator);
    m_searchRotation = 0.0f;
    m_searchDir = 1;

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

void Drone::Update(double dt, const Player& player, Math::Vec2 playerHitboxSize, bool isPlayerHiding)
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

        if (m_position.y < m_groundLevel + m_size.y / 2.0f)
        {
            m_position.y = m_groundLevel + m_size.y / 2.0f;
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

    if (distSq < (600.0f * 600.0f))
    {
        m_currentSpeed = m_baseSpeed * 1.5f;
    }
    else
    {
        m_currentSpeed = m_baseSpeed;
    }

    float playerHitboxRadius = (playerHitboxSize.x + playerHitboxSize.y) * 0.15f;
    float effectiveDetectionRange = DETECTION_RANGE + playerHitboxRadius;
    float effectiveDetectionRangeSq = effectiveDetectionRange * effectiveDetectionRange;

    bool canDetectPlayer = !isPlayerHiding;

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

        if (!canDetectPlayer)
        {
            m_isAttacking = false;
            m_position = m_attackStartPos;
            m_attackTimer = 0.0f;
        }
    }
    else if (canDetectPlayer && distSq < effectiveDetectionRangeSq && m_attackCooldown <= 0.0f)
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
        if (m_isTracer)
        {
            Math::Vec2 targetVelocity;
            if (canDetectPlayer)
            {
                m_isChasing = true;
                m_lostTimer = 0.0f;
                m_direction = toPlayer.GetNormalized();
                targetVelocity = m_direction * m_currentSpeed;

                m_searchRotation = 0.0f;
                m_searchDir = 1;
            }
            else
            {
                if (m_isChasing)
                {
                    m_isChasing = false;
                    m_lostTimer = 1.0f;
                }

                if (m_lostTimer > 0.0f)
                {
                    m_lostTimer -= static_cast<float>(dt);

                    const float searchRotationSpeed = 120.0f;
                    m_searchRotation += m_searchDir * searchRotationSpeed * static_cast<float>(dt);

                    if (m_searchDir == 1 && m_searchRotation > m_searchMaxAngle)
                    {
                        m_searchRotation = m_searchMaxAngle;
                        m_searchDir = -1;
                    }
                    else if (m_searchDir == -1 && m_searchRotation < -m_searchMaxAngle)
                    {
                        m_searchRotation = -m_searchMaxAngle;
                        m_searchDir = 1;
                    }

                    targetVelocity = { 0.0f, 0.0f };
                }
                else
                {
                    m_direction = { 1.0f, 0.0f };
                    targetVelocity = m_direction * m_currentSpeed;

                    m_searchRotation = 0.0f;
                    m_searchDir = 1;
                }
            }

            Math::Vec2 diff = targetVelocity - m_velocity;
            float maxChange = m_acceleration * static_cast<float>(dt);
            if (diff.LengthSq() > maxChange * maxChange)
            {
                diff = diff.GetNormalized() * maxChange;
            }
            m_velocity += diff;
            m_position += m_velocity * static_cast<float>(dt);
        }
        else
        {
            m_moveTimer += static_cast<float>(dt);
            if (m_moveTimer > 5.0f)
            {
                m_moveTimer = 0.0f;
                m_direction.x = -m_direction.x;
            }

            m_bobTimer += static_cast<float>(dt);

            float targetVelX = m_direction.x * m_currentSpeed;
            float accel = m_acceleration * static_cast<float>(dt);

            if (m_velocity.x < targetVelX)
            {
                m_velocity.x += accel;
                if (m_velocity.x > targetVelX) m_velocity.x = targetVelX;
            }
            else if (m_velocity.x > targetVelX)
            {
                m_velocity.x -= accel;
                if (m_velocity.x < targetVelX) m_velocity.x = targetVelX;
            }

            m_velocity.y = 0.f;
            m_position.x += m_velocity.x * static_cast<float>(dt);

            const float bobAmplitude = 20.0f;
            const float bobFrequency = 3.0f;

            float bobOffset = std::sin(m_bobTimer * bobFrequency) * bobAmplitude;

            m_position.y = m_baseY + bobOffset;
        }
    }
}

void Drone::Draw(const Shader& shader) const
{
    Math::Matrix rotationMatrix = Math::Matrix::CreateIdentity();
    bool flipX = false;

    if (m_isHit || m_isDead)
    {
        float wobble = 0.0f;
        if (m_isHit)
        {
            wobble = std::sin(m_hitTimer * 20.0f) * 45.0f;
        }
        rotationMatrix = Math::Matrix::CreateRotation(m_hitRotation + wobble);
    }
    else if (m_isAttacking)
    {
        float tiltAngle = m_attackAngle + 90.0f * m_attackDirection;
        rotationMatrix = Math::Matrix::CreateRotation(tiltAngle);
    }
    else if (m_isTracer)
    {
        if (m_lostTimer > 0.0f)
        {
            flipX = (m_direction.x < 0.0f);

            float baseAngle = 0.0f;
            if (flipX) {
                baseAngle = std::atan2(m_direction.y, -m_direction.x) * (180.0f / PI);
            }
            else {
                baseAngle = std::atan2(m_direction.y, m_direction.x) * (180.0f / PI);
            }

            rotationMatrix = Math::Matrix::CreateRotation(baseAngle + m_searchRotation);
        }
        else if (m_velocity.LengthSq() > 0.01f)
        {
            if (m_velocity.x < 0.0f)
            {
                flipX = true;
                float angle = std::atan2(m_velocity.y, -m_velocity.x) * (180.0f / PI);
                rotationMatrix = Math::Matrix::CreateRotation(angle);
            }
            else
            {
                flipX = false;
                float angle = std::atan2(m_velocity.y, m_velocity.x) * (180.0f / PI);
                rotationMatrix = Math::Matrix::CreateRotation(angle);
            }
        }
        else
        {
            flipX = (m_direction.x < 0.0f);
            float angle = 0.0f;
            if (flipX) {
                angle = std::atan2(m_direction.y, -m_direction.x) * (180.0f / PI);
            }
            else {
                angle = std::atan2(m_direction.y, m_direction.x) * (180.0f / PI);
            }
            rotationMatrix = Math::Matrix::CreateRotation(angle);
        }
    }
    else
    {
        flipX = (m_direction.x < 0.0f);
    }

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(m_size);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation(m_position);
    Math::Matrix model = transMatrix * rotationMatrix * scaleMatrix;

    shader.use();
    shader.setMat4("model", model);
    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", flipX);

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

void Drone::StartDeathSequence()
{
    if (m_isHit || m_isDead) return;

    m_isHit = true;
    m_hitTimer = m_hitDuration;
    m_hitRotation = 0.0f;
    m_fallSpeed = 0.0f;
    m_isAttacking = false;

    Logger::Instance().Log(Logger::Severity::Event, "Drone hit! Starting death sequence.");
}

bool Drone::ApplyDamage(float dt)
{
    if (m_isHit || m_isDead) return false;

    m_damageTimer += dt;
    if (m_damageTimer >= TIME_TO_DESTROY)
    {
        StartDeathSequence();
        return true;
    }
    return false;
}

void Drone::ResetDamageTimer()
{
    m_damageTimer = 0.0f;
}