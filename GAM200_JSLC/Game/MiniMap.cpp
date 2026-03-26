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

namespace
{
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

constexpr int PANEL_SIZE = 320;
constexpr int PANEL_MARGIN = 30;

constexpr float VIEW_WORLD_WIDTH = 1100.0f;
constexpr float VIEW_WORLD_HEIGHT = VIEW_WORLD_WIDTH;
constexpr float MOVING_SPEED_SQ = 25.0f;
}

MiniMap::EnemyFocus MiniMap::FindClosestMovingEnemy(const Math::Vec2& playerPos, Hallway& hallway, Rooftop& rooftop,
                                                     Underground& underground, Subway& subway,
                                                     DroneManager& mainDroneManager, bool undergroundAccessed,
                                                     bool subwayAccessed) const
{
    EnemyFocus result{};
    result.distSq = std::numeric_limits<float>::max();

    auto considerPos = [&](const Math::Vec2& pos) {
        float d = (pos - playerPos).LengthSq();
        if (d < result.distSq)
        {
            result.valid = true;
            result.pos = pos;
            result.distSq = d;
        }
    };

    auto considerDrones = [&](const std::vector<Drone>& drones) {
        for (const auto& d : drones)
        {
            if (d.IsDead()) continue;
            if (d.GetVelocity().LengthSq() <= MOVING_SPEED_SQ) continue;
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
            if (r.GetVelocity().LengthSq() <= MOVING_SPEED_SQ) continue;
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
    EnemyFocus focus = FindClosestMovingEnemy(playerPos, hallway, rooftop, underground, subway, mainDroneManager, undergroundAccessed, subwayAccessed);
    Math::Vec2 cameraCenter = focus.valid ? focus.pos : playerPos;

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
    room.Draw(textureShader);
    hallway.Draw(textureShader);
    rooftop.Draw(textureShader);
    underground.Draw(textureShader);
    subway.Draw(textureShader);
    mainDroneManager.Draw(textureShader);
    player.Draw(textureShader);

    GL::Disable(GL_SCISSOR_TEST);
    GL::Viewport(0, 0, static_cast<int>(GAME_WIDTH), static_cast<int>(GAME_HEIGHT));

    std::string mapName = GetCurrentMapName(playerPos);
    CachedTextureInfo mapText = font.PrintToTexture(fontShader, mapName);
    float textHeight = 24.0f;
    float textWidth = (mapText.height > 0) ? (static_cast<float>(mapText.width) * (textHeight / static_cast<float>(mapText.height))) : 0.0f;
    float textX = panelX + (PANEL_SIZE - textWidth) * 0.5f;
    float textY = panelY + 8.0f;

    fontShader.use();
    fontShader.setMat4("projection", baseProjection);
    font.DrawBakedText(fontShader, mapText, { textX, textY }, textHeight);
}
