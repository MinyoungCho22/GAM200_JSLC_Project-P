#include "MiniMap.hpp"

#include "Player.hpp"
#include "Room.hpp"
#include "Hallway.hpp"
#include "Rooftop.hpp"
#include "Underground.hpp"
#include "Subway.hpp"
#include "DroneManager.hpp"
#include "Drone.hpp"
#include "Robot.hpp"
#include "../OpenGL/Shader.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/DebugRenderer.hpp"

#include <limits>
#include <algorithm>

namespace
{
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

constexpr int PANEL_SIZE = 320;
constexpr int PANEL_MARGIN = 30;

constexpr float VIEW_WORLD_WIDTH = 1100.0f;
constexpr float VIEW_WORLD_HEIGHT = VIEW_WORLD_WIDTH;

enum class ActiveMap
{
    Room,
    Hallway,
    Rooftop,
    Underground,
    Subway
};

struct MapView
{
    ActiveMap type{};
    float minX = 0.0f;
    float maxX = 0.0f;
    float minY = 0.0f;
    float maxY = 0.0f;
};

MapView ResolveMapView(const Math::Vec2& playerPos)
{
    if (playerPos.y >= Rooftop::MIN_Y)
        return { ActiveMap::Rooftop, Rooftop::MIN_X, Rooftop::MIN_X + Rooftop::WIDTH, Rooftop::MIN_Y, Rooftop::MIN_Y + Rooftop::HEIGHT };

    if (playerPos.y <= Subway::MIN_Y + Subway::HEIGHT)
        return { ActiveMap::Subway, Subway::MIN_X, Subway::MIN_X + Subway::WIDTH, Subway::MIN_Y, Subway::MIN_Y + Subway::HEIGHT };

    if (playerPos.y <= Underground::MIN_Y + Underground::HEIGHT)
        return { ActiveMap::Underground, Underground::MIN_X, Underground::MIN_X + Underground::WIDTH, Underground::MIN_Y, Underground::MIN_Y + Underground::HEIGHT };

    if (playerPos.x < GAME_WIDTH)
        return { ActiveMap::Room, 0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT };

    return { ActiveMap::Hallway, GAME_WIDTH, GAME_WIDTH + Hallway::WIDTH, 0.0f, Hallway::HEIGHT };
}

bool IsInsideMap(const Math::Vec2& p, const MapView& view)
{
    return p.x >= view.minX && p.x <= view.maxX && p.y >= view.minY && p.y <= view.maxY;
}

bool TryGetNextMapType(ActiveMap current, ActiveMap& next)
{
    switch (current)
    {
    case ActiveMap::Room:        next = ActiveMap::Hallway; return true;
    case ActiveMap::Hallway:     next = ActiveMap::Rooftop; return true;
    case ActiveMap::Rooftop:     next = ActiveMap::Underground; return true;
    case ActiveMap::Underground: next = ActiveMap::Subway; return true;
    case ActiveMap::Subway:      return false;
    }
    return false;
}

MapView GetMapViewByType(ActiveMap type)
{
    switch (type)
    {
    case ActiveMap::Room:        return { ActiveMap::Room, 0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT };
    case ActiveMap::Hallway:     return { ActiveMap::Hallway, GAME_WIDTH, GAME_WIDTH + Hallway::WIDTH, 0.0f, Hallway::HEIGHT };
    case ActiveMap::Rooftop:     return { ActiveMap::Rooftop, Rooftop::MIN_X, Rooftop::MIN_X + Rooftop::WIDTH, Rooftop::MIN_Y, Rooftop::MIN_Y + Rooftop::HEIGHT };
    case ActiveMap::Underground: return { ActiveMap::Underground, Underground::MIN_X, Underground::MIN_X + Underground::WIDTH, Underground::MIN_Y, Underground::MIN_Y + Underground::HEIGHT };
    case ActiveMap::Subway:      return { ActiveMap::Subway, Subway::MIN_X, Subway::MIN_X + Subway::WIDTH, Subway::MIN_Y, Subway::MIN_Y + Subway::HEIGHT };
    }
    return { ActiveMap::Room, 0.0f, GAME_WIDTH, 0.0f, GAME_HEIGHT };
}

const char* GetMapNameFromType(ActiveMap type)
{
    switch (type)
    {
    case ActiveMap::Room: return "ROOM";
    case ActiveMap::Hallway: return "HALLWAY";
    case ActiveMap::Rooftop: return "ROOFTOP";
    case ActiveMap::Underground: return "UNDERGROUND";
    case ActiveMap::Subway: return "SUBWAY";
    }
    return "ROOM";
}
}

MiniMap::EnemyFocus MiniMap::FindClosestMovingEnemy(const Math::Vec2& playerPos, Hallway& hallway, Rooftop& rooftop,
                                                     Underground& underground, Subway& subway,
                                                     DroneManager& mainDroneManager, bool undergroundAccessed,
                                                     bool subwayAccessed) const
{
    EnemyFocus result{};
    result.distSq = (std::numeric_limits<float>::max)();

    auto considerPos = [&](const Math::Vec2& pos) {
        float d = (pos - playerPos).LengthSq();
        if (d < result.distSq)
        {
            result.valid = true;
            result.pos = pos;
            result.distSq = d;
        }
    };

    MapView currentMap = ResolveMapView(playerPos);

    auto considerDrones = [&](const std::vector<Drone>& drones) {
        for (const auto& d : drones)
        {
            if (d.IsDead()) continue;
            if (!IsInsideMap(d.GetPosition(), currentMap)) continue;
            considerPos(d.GetPosition());
        }
    };

    // If tracers are active, prioritize nearest tracer to player.
    for (const auto& d : mainDroneManager.GetDrones())
    {
        if (d.IsDead() || !d.IsTracer()) continue;
        considerPos(d.GetPosition());
    }
    if (result.valid)
    {
        return result;
    }

    auto considerRobots = [&](const std::vector<Robot>& robots) {
        for (const auto& r : robots)
        {
            if (r.IsDead()) continue;
            if (!IsInsideMap(r.GetPosition(), currentMap)) continue;
            considerPos(r.GetPosition());
        }
    };

    considerDrones(mainDroneManager.GetDrones());
    considerDrones(hallway.GetDrones());
    considerDrones(rooftop.GetDrones());

    if (undergroundAccessed)
    {
        considerDrones(underground.GetDrones());
        considerRobots(underground.GetRobots());
    }

    if (subwayAccessed)
    {
        considerDrones(subway.GetDrones());
        considerRobots(subway.GetRobots());
    }

    return result;
}

std::string MiniMap::GetCurrentMapName(const Math::Vec2& playerPos) const
{
    if (playerPos.y >= Rooftop::MIN_Y) return "ROOFTOP";
    if (playerPos.y <= Subway::MIN_Y + Subway::HEIGHT) return "SUBWAY";
    if (playerPos.y <= Underground::MIN_Y + Underground::HEIGHT) return "UNDERGROUND";
    if (playerPos.x < GAME_WIDTH) return "ROOM";
    return "HALLWAY";
}

void MiniMap::Draw(Shader& textureShader, Shader& colorShader, Shader& fontShader, Font& font, DebugRenderer& debugRenderer,
                   const Player& player, Room& room, Hallway& hallway, Rooftop& rooftop,
                   Underground& underground, Subway& subway, DroneManager& mainDroneManager,
                   bool undergroundAccessed, bool subwayAccessed, const Math::Matrix& baseProjection)
{
    // Preserve GL state that we temporarily override for minimap rendering.
    GLint prevViewport[4] = { 0, 0, 0, 0 };
    GLint prevScissorBox[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);

    const int panelX = static_cast<int>(GAME_WIDTH) - PANEL_MARGIN - PANEL_SIZE;
    const int panelY = static_cast<int>(GAME_HEIGHT) - PANEL_MARGIN - PANEL_SIZE;

    colorShader.use();
    colorShader.setMat4("projection", baseProjection);
    colorShader.setFloat("uAlpha", 0.85f);
    debugRenderer.DrawBox(colorShader,
                          { panelX + PANEL_SIZE * 0.5f, panelY + PANEL_SIZE * 0.5f },
                          { static_cast<float>(PANEL_SIZE + 10), static_cast<float>(PANEL_SIZE + 10) },
                          { 0.05f, 0.05f });

    colorShader.setFloat("uAlpha", 1.0f);
    debugRenderer.DrawBox(colorShader,
                          { panelX + PANEL_SIZE * 0.5f, panelY + PANEL_SIZE * 0.5f },
                          { static_cast<float>(PANEL_SIZE), static_cast<float>(PANEL_SIZE) },
                          { 0.8f, 0.8f });

    Math::Vec2 playerPos = player.GetPosition();
    MapView currentMap = ResolveMapView(playerPos);
    MapView displayMap = currentMap;

    auto findClosestInMap = [&](const MapView& mapView) {
        EnemyFocus f{};
        f.distSq = (std::numeric_limits<float>::max)();

        auto considerPos = [&](const Math::Vec2& pos) {
            if (!IsInsideMap(pos, mapView)) return;
            float d = (pos - playerPos).LengthSq();
            if (d < f.distSq)
            {
                f.valid = true;
                f.pos = pos;
                f.distSq = d;
            }
        };

        for (const auto& d : mainDroneManager.GetDrones()) if (!d.IsDead()) considerPos(d.GetPosition());
        for (const auto& d : hallway.GetDrones()) if (!d.IsDead()) considerPos(d.GetPosition());
        for (const auto& d : rooftop.GetDrones()) if (!d.IsDead()) considerPos(d.GetPosition());
        for (const auto& d : underground.GetDrones()) if (!d.IsDead()) considerPos(d.GetPosition());
        for (const auto& d : subway.GetDrones()) if (!d.IsDead()) considerPos(d.GetPosition());
        for (const auto& r : underground.GetRobots()) if (!r.IsDead()) considerPos(r.GetPosition());
        for (const auto& r : subway.GetRobots()) if (!r.IsDead()) considerPos(r.GetPosition());
        return f;
    };

    EnemyFocus focus = findClosestInMap(currentMap);
    if (!focus.valid)
    {
        ActiveMap nextType{};
        if (TryGetNextMapType(currentMap.type, nextType))
        {
            MapView nextMap = GetMapViewByType(nextType);
            EnemyFocus nextFocus = findClosestInMap(nextMap);
            if (nextFocus.valid)
            {
                focus = nextFocus;
                displayMap = nextMap;
            }
        }
    }

    Math::Vec2 targetCenter = focus.valid
        ? focus.pos
        : Math::Vec2{ (currentMap.minX + currentMap.maxX) * 0.5f, (currentMap.minY + currentMap.maxY) * 0.5f };

    // Smooth minimap camera target to avoid one-frame jumps when nearest normal drone changes.
    if (!m_hasSmoothedCenter)
    {
        m_smoothedCenter = targetCenter;
        m_hasSmoothedCenter = true;
    }
    else
    {
        float d2 = (targetCenter - m_smoothedCenter).LengthSq();
        if (d2 > 2500.0f * 2500.0f)
        {
            // Large teleports/map transitions: snap immediately.
            m_smoothedCenter = targetCenter;
        }
        else
        {
            const float smooth = 0.14f;
            m_smoothedCenter = m_smoothedCenter + (targetCenter - m_smoothedCenter) * smooth;
        }
    }
    Math::Vec2 cameraCenter = m_smoothedCenter;

    // Clamp minimap camera to displayed map bounds.
    const float halfW = VIEW_WORLD_WIDTH * 0.5f;
    const float halfH = VIEW_WORLD_HEIGHT * 0.5f;
    cameraCenter.x = (std::max)(displayMap.minX + halfW, (std::min)(displayMap.maxX - halfW, cameraCenter.x));
    cameraCenter.y = (std::max)(displayMap.minY + halfH, (std::min)(displayMap.maxY - halfH, cameraCenter.y));

    Math::Matrix miniProjection = Math::Matrix::CreateOrtho(
        cameraCenter.x - VIEW_WORLD_WIDTH * 0.5f, cameraCenter.x + VIEW_WORLD_WIDTH * 0.5f,
        cameraCenter.y - VIEW_WORLD_HEIGHT * 0.5f, cameraCenter.y + VIEW_WORLD_HEIGHT * 0.5f,
        -1.0f, 1.0f);

    GL::Enable(GL_SCISSOR_TEST);
    glScissor(panelX, panelY, PANEL_SIZE, PANEL_SIZE);
    GL::Viewport(panelX, panelY, PANEL_SIZE, PANEL_SIZE);
    GL::ClearColor(0.03f, 0.03f, 0.05f, 1.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    textureShader.use();
    textureShader.setMat4("projection", miniProjection);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);
    switch (displayMap.type)
    {
    case ActiveMap::Room:       room.Draw(textureShader); break;
    case ActiveMap::Hallway:    hallway.Draw(textureShader); break;
    case ActiveMap::Rooftop:    rooftop.Draw(textureShader); break;
    case ActiveMap::Underground:underground.Draw(textureShader); break;
    case ActiveMap::Subway:     subway.Draw(textureShader); break;
    }

    // Draw tracer/main drones only if they are in the displayed map bounds.
    for (const auto& d : mainDroneManager.GetDrones())
    {
        if (d.IsDead()) continue;
        if (!IsInsideMap(d.GetPosition(), displayMap)) continue;
        d.Draw(textureShader);
    }
    player.Draw(textureShader);

    // Restore viewport and force-disable scissor for subsequent world/fullscreen draws.
    GL::Viewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glScissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);
    GL::Disable(GL_SCISSOR_TEST);

    std::string mapName = GetMapNameFromType(displayMap.type);
    CachedTextureInfo mapText = font.PrintToTexture(fontShader, mapName);
    float textHeight = 24.0f;
    float textWidth = (mapText.height > 0) ? (static_cast<float>(mapText.width) * (textHeight / static_cast<float>(mapText.height))) : 0.0f;
    float textX = panelX + (PANEL_SIZE - textWidth) * 0.5f;
    float textY = panelY + 8.0f;

    fontShader.use();
    fontShader.setMat4("projection", baseProjection);
    font.DrawBakedText(fontShader, mapText, { textX, textY }, textHeight);
}
