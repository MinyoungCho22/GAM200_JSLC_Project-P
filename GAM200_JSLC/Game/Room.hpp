// Room.hpp

#pragma once
#include "Background.hpp"
#include "PulseSource.hpp"
#include "../Engine/Rect.hpp"
#include "../Engine/Input.hpp" 
#include <memory>
#include <vector>

class Engine;
class Shader;
class Player;
class DebugRenderer;

/**
 * @class Room
 * @brief Represents a contained indoor level environment with its own boundaries, 
 * interactive objects (PulseSources), and specific mechanics like "Blinds."
 */
class Room
{
public:
    void Initialize(Engine& engine, const char* texturePath);
    void Shutdown();
    void Update(Player& player, double dt, Input::Input& input, Math::Vec2 mouseWorldPos);
    void Draw(Shader& textureShader) const;
    
    Math::Vec2 GetBlindPos() const { return m_blindPos; }
    Math::Vec2 GetBlindSize() const { return m_blindSize; }
    
    // Renders visual representations of hitboxes and interactive zones
    void DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection, const Player& player) const;

    void SetRightBoundaryActive(bool active) { m_rightBoundaryActive = active; }
    Background* GetBackground() { return m_background.get(); }
    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }

    /**
     * @brief Checks if the player is currently hidden within a specific zone (e.g., behind blinds).
     */
    bool IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;

    bool IsPlayerInBlindArea() const { return m_playerInBlindArea; }
    bool IsBlindOpen() const { return m_isBright; }

private:
    std::unique_ptr<Background> m_background;       // Normal (dark) state background
    std::unique_ptr<Background> m_brightBackground; // Bright state background (blinds open)
    
    Math::Rect m_boundaries;     // Physical movement limits for the player
    Math::Vec2 m_roomCenter;
    Math::Vec2 m_roomSize;
    
    bool m_rightBoundaryActive = true; // Determines if the player can exit to the right
    std::vector<PulseSource> m_pulseSources;

    // Blind interaction zone properties
    Math::Vec2 m_blindPos;
    Math::Vec2 m_blindSize;

    bool m_isBright = false;           // Current lighting state of the room
    bool m_playerInBlindArea = false;  // Whether the player is overlapping with the blind trigger
};