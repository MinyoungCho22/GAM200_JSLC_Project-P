// Tutorial.cpp

#include "Tutorial.hpp"
#include "Player.hpp"
#include "Hallway.hpp"
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

    m_messages.push_back(msg);
}

void Tutorial::AddHidingSpotMessage(Font& font, Shader& atlasShader, Math::Vec2 hidingSpotPos, Math::Vec2 hidingSpotSize)
{
    TutorialMessage msg;
    msg.text = "Press the S key to hide inside the electromagnetic shield";
    msg.texture = font.PrintToTexture(atlasShader, msg.text);
    msg.targetPosition = hidingSpotPos;
    msg.targetSize = hidingSpotSize * 2.0f;
    msg.textPosition = { GAME_WIDTH / 2.0f - 450.0f, 100.0f };
    msg.textHeight = 50.0f;
    msg.isActive = false;

    m_messages.push_back(msg);
}

void Tutorial::Update(float dt, Player& player, const Input::Input& input, Hallway* hallway)
{
    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetHitboxSize();

    for (auto& msg : m_messages)
    {
        bool isColliding = Collision::CheckAABB(playerPos, playerSize, msg.targetPosition, msg.targetSize);
        msg.isActive = isColliding;
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