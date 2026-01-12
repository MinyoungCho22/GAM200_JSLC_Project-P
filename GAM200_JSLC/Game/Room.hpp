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

class Room
{
public:
    void Initialize(Engine& engine, const char* texturePath);
    void Shutdown();
    void Update(Player& player, double dt, Input::Input& input);
    void Draw(Shader& textureShader) const;
    void DrawDebug(DebugRenderer& renderer, Shader& colorShader, const Math::Matrix& projection, const Player& player) const;

    void SetRightBoundaryActive(bool active) { m_rightBoundaryActive = active; }
    Background* GetBackground() { return m_background.get(); }
    std::vector<PulseSource>& GetPulseSources() { return m_pulseSources; }

    bool IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const;

    bool IsPlayerInBlindArea() const { return m_playerInBlindArea; }
    bool IsBlindOpen() const { return m_isBright; }
    bool IsTVActivated() const { return m_isTVActivated; }
    bool CanProceed() const { return m_isBright && m_isTVActivated; }

private:
    std::unique_ptr<Background> m_background;
    std::unique_ptr<Background> m_brightBackground;
    Math::Rect m_boundaries;
    Math::Vec2 m_roomCenter;
    Math::Vec2 m_roomSize;
    bool m_rightBoundaryActive = true;
    std::vector<PulseSource> m_pulseSources;

    Math::Vec2 m_blindPos;
    Math::Vec2 m_blindSize;
    bool m_isBright = false;
    bool m_playerInBlindArea = false;

    Math::Vec2 m_tvPos;
    Math::Vec2 m_tvSize;
    bool m_isTVActivated = false;
    bool m_playerInTVArea = false;
};