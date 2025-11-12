// Door.cpp

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

    float interactRange = 200.0f;
    if (m_doorType == DoorType::HallwayToRooftop)
    {
        constexpr float ROOFTOP_TRIGGER_X_MIN = 7000.0f;
        constexpr float ROOFTOP_TRIGGER_X_MAX = 7250.0f;

        m_isPlayerNearby = (playerCenter.x > ROOFTOP_TRIGGER_X_MIN &&
            playerCenter.x < ROOFTOP_TRIGGER_X_MAX);
    }
    else
    {
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