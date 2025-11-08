// Door.cpp

#include "Door.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"

void Door::Initialize(Math::Vec2 position, Math::Vec2 size, float pulseCost)
{
    m_position = position;
    m_size = size;
    m_pulseCost = pulseCost;
    m_isPlayerNearby = false;
    m_shouldLoadNextMap = false;

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

void Door::Update(Player& player, bool isInteractKeyPressed)
{
    Math::Vec2 playerCenter = player.GetPosition();

    // 플레이어와 문 사이의 거리 계산
    float distSq = (playerCenter - m_position).LengthSq();
    constexpr float INTERACT_RANGE = 200.0f;
    constexpr float INTERACT_RANGE_SQ = INTERACT_RANGE * INTERACT_RANGE;

    // 사거리 내에 있는지 확인
    m_isPlayerNearby = (distSq < INTERACT_RANGE_SQ);

    if (m_isPlayerNearby && isInteractKeyPressed)
    {
        float currentPulse = player.GetPulseCore().getPulse().Value();

        if (currentPulse >= m_pulseCost)
        {
            player.GetPulseCore().getPulse().spend(m_pulseCost);
            m_shouldLoadNextMap = true;

            Logger::Instance().Log(Logger::Severity::Event,
                "Door opened! Pulse cost: %.1f, Remaining pulse: %.1f",
                m_pulseCost, player.GetPulseCore().getPulse().Value());
        }
        else
        {
            Logger::Instance().Log(Logger::Severity::Error,
                "Not enough pulse! Required: %.1f, Current: %.1f",
                m_pulseCost, currentPulse);
        }
    }
}

void Door::DrawDebug(Shader& shader) const
{
    Math::Matrix scale = Math::Matrix::CreateScale(m_size);
    Math::Matrix translation = Math::Matrix::CreateTranslation(m_position);
    Math::Matrix model = translation * scale;

    shader.setMat4("model", model);

    if (m_isPlayerNearby)
        shader.setVec3("objectColor", 0.0f, 1.0f, 0.0f);
    else
        shader.setVec3("objectColor", 1.0f, 1.0f, 0.0f);

    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Door::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
}