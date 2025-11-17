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
class Rooftop;
class Room;
class Door;

struct TutorialMessage
{
    enum class Type
    {
        Collision,
        RooftopHole,
        RoomBlind,
        DoorInteractionRoom,
        DoorInteractionRooftop,
        DroneCrashHint,
        LiftInteraction,
        LiftCooldown
    };

    std::string id;
    Type type = Type::Collision;
    std::string text;
    CachedTextureInfo texture;
    Math::Vec2 targetPosition;
    Math::Vec2 targetSize;
    Math::Vec2 textPosition;
    float textHeight;
    bool isActive;
    bool isPermanentlyDisabled = false;
    float timer = 0.0f;
    float duration = -1.0f;
};

class Tutorial
{
public:
    Tutorial();
    void Init(Font& font, Shader& atlasShader, Math::Vec2 pursePosition, Math::Vec2 purseSize);
    void AddHidingSpotMessage(Font& font, Shader& atlasShader, Math::Vec2 hidingSpotPos, Math::Vec2 hidingSpotSize);
    void AddHoleMessage(Font& font, Shader& atlasShader);
    void AddBlindMessage(Font& font, Shader& atlasShader);
    void AddRoomDoorMessage(Font& font, Shader& atlasShader);
    void AddRooftopDoorMessage(Font& font, Shader& atlasShader);
    void AddDroneCrashMessage(Font& font, Shader& atlasShader);
    void AddLiftMessage(Font& font, Shader& atlasShader);
    void AddLiftCooldownMessage(Font& font, Shader& atlasShader);
    void Update(float dt, Player& player, const Input::Input& input, Room* room = nullptr, Hallway* hallway = nullptr, Rooftop* rooftop = nullptr, Door* roomDoor = nullptr, Door* rooftopDoor = nullptr);
    void Draw(Font& font, Shader& textureShader);

private:
    std::vector<TutorialMessage> m_messages;
    float m_crouchTimer = 0.0f;
    bool m_crouchTutorialCompleted = false;
};