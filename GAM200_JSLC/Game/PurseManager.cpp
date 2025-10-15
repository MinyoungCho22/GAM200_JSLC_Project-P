#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"
#include "../Engine/Logger.hpp"

void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed)
{
    bool is_near_any_charger = false;
    PulseSource* nearby_source = nullptr;

    for (auto& source : sources)
    {
        if (!source.HasPulse()) continue;

        Math::Vec2 playerPos = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        float playerMinX = playerPos.x - playerSize.x / 2.0f;
        float playerMaxX = playerPos.x + playerSize.x / 2.0f;
        float playerMinY = playerPos.y;
        float playerMaxY = playerPos.y + playerSize.y;

        Math::Vec2 sourcePos = source.GetPosition();
        Math::Vec2 sourceSize = source.GetSize();
        float sourceMinX = sourcePos.x - sourceSize.x / 2.0f;
        float sourceMaxX = sourcePos.x + sourceSize.x / 2.0f;
        float sourceMinY = sourcePos.y - sourceSize.y / 2.0f;
        float sourceMaxY = sourcePos.y + sourceSize.y / 2.0f;

        if (playerMaxX > sourceMinX && playerMinX < sourceMaxX &&
            playerMaxY > sourceMinY && playerMinY < sourceMaxY)
        {
            is_near_any_charger = true;
            nearby_source = &source;
            Logger::Instance().Log(Logger::Severity::Debug, "Collision Detected with a pulse source!");
            break;
        }
    }

    // [수정] isDashing 인자를 tick 함수에 전달
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_any_charger, player.IsDashing());

    if (result.charged && nearby_source != nullptr)
    {
        Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Delta: %f", result.delta);
        nearby_source->Drain(result.delta);
    }
}