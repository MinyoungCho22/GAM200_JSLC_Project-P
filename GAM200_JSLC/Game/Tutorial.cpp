// Tutorial.cpp

#include "Tutorial.hpp"
#include "Player.hpp"
#include "Hallway.hpp"
#include "Rooftop.hpp"
#include "Room.hpp"
#include "Door.hpp"
#include "../Engine/Input.hpp"       
#include "../Engine/Collision.hpp"    
#include "../OpenGL/Shader.hpp" 

constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

Tutorial::Tutorial()
{
}

void Tutorial::Init(Font& font, Shader& atlasShader, Math::Vec2 pursePosition, Math::Vec2 purseSize)
{
    TutorialMessage msg;
    msg.text = "Press E To Absorb the Purse";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.targetPosition = pursePosition;
    msg.targetSize = purseSize;
    msg.textPosition = { GAME_WIDTH / 2.0f - 250.0f, 100.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::Collision;

    m_messages.push_back(msg);
}

void Tutorial::AddHidingSpotMessage(Font& font, Shader& atlasShader, Math::Vec2 hidingSpotPos, Math::Vec2 hidingSpotSize)
{
    TutorialMessage msg;
    msg.id = "hiding_spot";
    msg.text = "Press the S key to hide inside the electromagnetic shield";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.targetPosition = hidingSpotPos;
    msg.targetSize = hidingSpotSize * 2.0f;
    msg.textPosition = { GAME_WIDTH / 2.0f - 450.0f, 100.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::Collision;

    m_messages.push_back(msg);
}

void Tutorial::AddHoleMessage(Font& font, Shader& atlasShader)
{
    TutorialMessage msg;
    msg.text = "Press the F key to fill the hole with pulses";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.textPosition = { GAME_WIDTH / 2.0f - 400.0f, 100.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::RooftopHole;

    m_messages.push_back(msg);
}

void Tutorial::AddBlindMessage(Font& font, Shader& atlasShader)
{
    TutorialMessage msg;
    msg.text = "Press the F key to open the blinds";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.textPosition = { GAME_WIDTH / 2.0f - 350.0f, 150.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::RoomBlind;

    m_messages.push_back(msg);
}

void Tutorial::AddRoomDoorMessage(Font& font, Shader& atlasShader)
{
    TutorialMessage msg;
    msg.text = "Press the F key to open the door.";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.textPosition = { GAME_WIDTH / 2.0f - 300.0f, 200.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::DoorInteractionRoom;

    m_messages.push_back(msg);
}

void Tutorial::AddRooftopDoorMessage(Font& font, Shader& atlasShader)
{
    TutorialMessage msg;
    msg.text = "Press the F key to go up the stairs.";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.textPosition = { GAME_WIDTH / 2.0f - 300.0f, 200.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::DoorInteractionRooftop;

    m_messages.push_back(msg);
}

void Tutorial::AddDroneCrashMessage(Font& font, Shader& atlasShader)
{
    TutorialMessage msg;
    msg.id = "drone_crash_hint";
    msg.text = "Hold down the F key for over 1 second to crash the drone!";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.textPosition = { GAME_WIDTH / 2.0f - 450.0f, 100.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::DroneCrashHint;
    msg.duration = 4.0f;
    msg.timer = 0.0f;

    m_messages.push_back(msg);
}

void Tutorial::AddLiftMessage(Font& font, Shader& atlasShader)
{
    TutorialMessage msg;
    msg.text = "Press the F key to take the Lift and move to another building";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.textPosition = { GAME_WIDTH / 2.0f - 500.0f, 100.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;
    msg.type = TutorialMessage::Type::LiftInteraction;

    m_messages.push_back(msg);
}

void Tutorial::Update(float dt, Player& player, const Input::Input& input, Room* room, Hallway* hallway, Rooftop* rooftop, Door* roomDoor, Door* rooftopDoor)
{
    if (!m_crouchTutorialCompleted && input.IsKeyPressed(Input::Key::S))
    {
        m_crouchTimer += dt;
    }
    else
    {
        m_crouchTimer = 0.0f;
    }

    if (!m_crouchTutorialCompleted && m_crouchTimer >= 2.0f)
    {
        m_crouchTutorialCompleted = true;

        for (auto& msg : m_messages)
        {
            if (msg.id == "hiding_spot")
            {
                msg.isPermanentlyDisabled = true;
                break;
            }
        }

        for (auto& msg : m_messages)
        {
            if (msg.id == "drone_crash_hint")
            {
                msg.isActive = true;
                msg.timer = msg.duration;
                break;
            }
        }
    }

    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetHitboxSize();

    for (auto& msg : m_messages)
    {
        if (msg.isPermanentlyDisabled)
        {
            msg.isActive = false;
            continue;
        }

        if (msg.type == TutorialMessage::Type::DroneCrashHint)
        {
            if (msg.isActive)
            {
                msg.timer -= dt;
                if (msg.timer <= 0.0f)
                {
                    msg.isActive = false;
                    msg.isPermanentlyDisabled = true;
                }
            }
        }
        else
        {
            switch (msg.type)
            {
            case TutorialMessage::Type::Collision:
            {
                bool isColliding = Collision::CheckAABB(playerPos, playerSize, msg.targetPosition, msg.targetSize);
                msg.isActive = isColliding;
                break;
            }
            case TutorialMessage::Type::RooftopHole:
            {
                msg.isActive = (rooftop != nullptr) && rooftop->IsPlayerCloseToHole();
                break;
            }
            case TutorialMessage::Type::RoomBlind:
            {
                msg.isActive = (room != nullptr) && room->IsPlayerInBlindArea();
                break;
            }
            case TutorialMessage::Type::DoorInteractionRoom:
            {
                msg.isActive = (roomDoor != nullptr && roomDoor->IsPlayerNearby());
                break;
            }
            case TutorialMessage::Type::DoorInteractionRooftop:
            {
                msg.isActive = (rooftopDoor != nullptr && rooftopDoor->IsPlayerNearby());
                break;
            }
            case TutorialMessage::Type::LiftInteraction:
            {
                msg.isActive = (rooftop != nullptr) && rooftop->IsPlayerNearLift();
                break;
            }
            default:
                break;
            }
        }
    }
}

void Tutorial::Draw(Font& font, Shader& textureShader)
{
    for (const auto& msg : m_messages)
    {
        if (msg.isActive)
        {
            font.DrawBakedText(textureShader, msg.texture, msg.textPosition, msg.textHeight);
        }
    }
}