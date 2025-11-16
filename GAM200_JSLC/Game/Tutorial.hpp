#pragma once

#include <string>
#include "../Engine/Vec2.hpp"
#include "Font.hpp" 

namespace Input { class Input; }
class Player;
class Shader;
class Font;

class Tutorial
{
public:
    Tutorial();

    void Init(Font& font, Shader& atlasShader, Math::Vec2 pursePosition, Math::Vec2 purseSize);

    void Update(float dt, Player& player, const Input::Input& input);

    void Draw(Font& font, Shader& textureShader);


private:

    std::string         m_popupText;
    CachedTextureInfo   m_popupTexture;
    Math::Vec2          m_textPosition;
    float               m_textHeight;

    Math::Vec2          m_pursePosition;
    Math::Vec2          m_purseSize;
    bool                m_canAbsorb;
};