// Tutorial.hpp

#pragma once
#include <string>
#include <vector>
#include "../Engine/Vec2.hpp"
#include "Font.hpp" 

namespace Input { class Input; }
class Player;
class Shader;
class Font;
class Hallway;

struct TutorialMessage
{
    std::string text;
    CachedTextureInfo texture;
    Math::Vec2 targetPosition;
    Math::Vec2 targetSize;
    Math::Vec2 textPosition;
    float textHeight;
    bool isActive;
};

class Tutorial
{
public:
    Tutorial();
    void Init(Font& font, Shader& atlasShader, Math::Vec2 pursePosition, Math::Vec2 purseSize);
    void AddHidingSpotMessage(Font& font, Shader& atlasShader, Math::Vec2 hidingSpotPos, Math::Vec2 hidingSpotSize);
    void Update(float dt, Player& player, const Input::Input& input, Hallway* hallway = nullptr);
    void Draw(Font& font, Shader& textureShader);

private:
    std::vector<TutorialMessage> m_messages;
};