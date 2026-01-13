//Tutorial.hpp

#pragma once
#include <string>
#include <vector>
#include "../Engine/Vec2.hpp"
#include "Font.hpp" 

// Forward declarations
namespace Input { class Input; }
class Player;
class Shader;
class Font;
class Hallway;
class Rooftop;
class Room;
class Door;

/**
 * @struct TutorialMessage
 * @brief Represents an individual tutorial UI element and its trigger conditions.
 */
struct TutorialMessage
{
    enum class Type
    {
        Collision,              // Triggered when player enters a specific area
        RooftopHole,            // Contextual: near a rooftop gap
        RoomBlind,              // Contextual: near room blinds
        DoorInteractionRoom,    // Contextual: near a closed room door
        DoorInteractionRooftop, // Contextual: near rooftop stairs
        DroneCrashHint,         // Timed: displayed after a specific action
        LiftInteraction         // Contextual: near the lift
    };

    std::string id = "";        // Unique identifier for logic tracking
    Type type = Type::Collision;
    std::string text = "";      // Content of the tutorial message
    CachedTextureInfo texture;  // Pre-rendered text texture info
    
    // Position/Size for area-based triggers
    Math::Vec2 targetPosition = { 0.0f, 0.0f };
    Math::Vec2 targetSize = { 0.0f, 0.0f };
    
    // Rendering parameters
    Math::Vec2 textPosition = { 0.0f, 0.0f };
    float textHeight = 0.0f;
    
    // State flags
    bool isActive = false;
    bool isPermanentlyDisabled = false; // Prevents message from reappearing once completed
    
    // Timer variables for duration-based display
    float timer = 0.0f;
    float duration = -1.0f;
};

/**
 * @class Tutorial
 * @brief Manages the initialization, logic, and rendering of the game's tutorial system.
 */
class Tutorial
{
public:
    Tutorial();
    
    // Initialization and message registration methods
    void Init(Font& font, Shader& atlasShader, Math::Vec2 pursePosition, Math::Vec2 purseSize);
    void AddHidingSpotMessage(Font& font, Shader& atlasShader, Math::Vec2 hidingSpotPos, Math::Vec2 hidingSpotSize);
    void AddHoleMessage(Font& font, Shader& atlasShader);
    void AddBlindMessage(Font& font, Shader& atlasShader);
    void AddRoomDoorMessage(Font& font, Shader& atlasShader);
    void AddRooftopDoorMessage(Font& font, Shader& atlasShader);
    void AddDroneCrashMessage(Font& font, Shader& atlasShader);
    void AddLiftMessage(Font& font, Shader& atlasShader);
    
    void DisableAll();
    
    /**
     * @brief Updates the tutorial state based on player position and world context.
     */
    void Update(float dt, Player& player, const Input::Input& input, 
                Room* room = nullptr, Hallway* hallway = nullptr, 
                Rooftop* rooftop = nullptr, Door* roomDoor = nullptr, 
                Door* rooftopDoor = nullptr);
    
    /**
     * @brief Renders active tutorial messages using the provided font and shader.
     */
    void Draw(Font& font, Shader& textureShader);

private:
    std::vector<TutorialMessage> m_messages; // Container for all registered tutorial steps
    
    // Specific state tracking for the crouch/hide tutorial
    float m_crouchTimer = 0.0f;
    bool m_crouchTutorialCompleted = false;
};