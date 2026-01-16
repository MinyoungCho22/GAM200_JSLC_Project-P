//Door.cpp

#include "Door.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"

void Door::Initialize(Math::Vec2 position, Math::Vec2 size, float pulseCost, DoorType type)
{
    m_position = position;
    m_size = size;
    m_pulseCost = pulseCost;
    m_doorType = type;
    m_isPlayerNearby = false;
    m_shouldLoadNextMap = false;
    m_isOpened = false;

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
    if (m_isOpened) return;

    Math::Vec2 playerCenter = player.GetPosition();

    if (m_doorType == DoorType::HallwayToRooftop)
    {
        float minX = m_position.x - m_size.x / 2.0f;
        float maxX = m_position.x + m_size.x / 2.0f;
        float minY = m_position.y - m_size.y / 2.0f;
        float maxY = m_position.y + m_size.y / 2.0f;

        m_isPlayerNearby = (playerCenter.x > minX && playerCenter.x < maxX &&
            playerCenter.y > minY && playerCenter.y < maxY);
    }
    else
    {
        float interactRange = 200.0f;
        float distSq = (playerCenter - m_position).LengthSq();
        float interactRangeSq = interactRange * interactRange;
        m_isPlayerNearby = (distSq < interactRangeSq);
    }

    if (m_isPlayerNearby && isInteractKeyPressed)
    {
        float currentPulse = player.GetPulseCore().getPulse().Value();
        if (currentPulse >= m_pulseCost)
        {
            player.GetPulseCore().getPulse().spend(m_pulseCost);
            m_shouldLoadNextMap = true;
            m_isOpened = true;

            if (m_doorType == DoorType::HallwayToRooftop)
            {
                Logger::Instance().Log(Logger::Severity::Event, "Rooftop Accessed!");
            }
            else
            {
                Logger::Instance().Log(Logger::Severity::Event,
                    "Door opened! Pulse cost: %.1f, Remaining pulse: %.1f",
                    m_pulseCost, player.GetPulseCore().getPulse().Value());
            }
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

    if (m_isOpened)
        shader.setVec3("objectColor", 0.5f, 0.5f, 0.5f);
    else if (m_isPlayerNearby)
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