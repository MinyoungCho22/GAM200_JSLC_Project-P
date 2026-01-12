#include "Door.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/QuadMesh.hpp"
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
}

void Door::Update(Player& player, bool isInteractKeyPressed, bool canProceed)
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
        // RoomToHallway 문은 TV와 블라인드에 펄스 주입해야 열 수 있음
        if (m_doorType == DoorType::RoomToHallway && !canProceed)
        {
            Logger::Instance().Log(Logger::Severity::Info,
                "You must inject pulse into the TV and blinds to open the door.");
            return;
        }

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

    OpenGL::QuadMesh::Bind();
    OpenGL::QuadMesh::Draw();
    OpenGL::QuadMesh::Unbind();
}

void Door::Shutdown()
{
}