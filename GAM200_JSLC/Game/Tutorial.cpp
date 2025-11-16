#include "Tutorial.hpp"
#include "Player.hpp"
#include "../Engine/Input.hpp"       
#include "../Engine/Collision.hpp"    
#include "../OpenGL/Shader.hpp" 

constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

Tutorial::Tutorial()
    : m_popupText("Press E To Absorb the Purse"),
    m_textPosition(GAME_WIDTH / 2.0f - 250.0f, 100.0f),
    m_textHeight(50.0f),
    m_pursePosition(0.0f, 0.0f),
    m_purseSize(0.0f, 0.0f),
    m_canAbsorb(false),
    m_popupTexture{}
{
}

void Tutorial::Init(Font& font, Shader& atlasShader, Math::Vec2 pursePosition, Math::Vec2 purseSize)
{
    m_pursePosition = pursePosition;
    m_purseSize = purseSize;

    m_popupTexture = font.PrintToTexture(atlasShader, m_popupText);
}

void Tutorial::Update(float dt, Player& player, const Input::Input& input)
{
    bool isColliding = Collision::CheckAABB(player.GetPosition(), player.GetSize(),
        m_pursePosition, m_purseSize);

    if (isColliding)
    {
        m_canAbsorb = true;
    }
    else
    {
        m_canAbsorb = false;
    }
}

void Tutorial::Draw(Font& font, Shader& textureShader)
{
    if (m_canAbsorb)
    {
        font.DrawBakedText(textureShader, m_popupTexture, m_textPosition, m_textHeight);
    }
}
