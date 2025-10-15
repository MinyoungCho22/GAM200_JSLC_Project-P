#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp" // Vec2의 거리 계산을 위해 추가

void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed, double dt)
{
    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f; // 가장 가까운 거리를 추적하기 위한 변수 (제곱값으로 비교)

    // 1. 플레이어와 충돌하는 모든 공급원을 확인합니다.
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
            // 2. 충돌이 감지되면, 플레이어와의 거리를 계산합니다.
            Math::Vec2 player_center = { playerPos.x, playerPos.y + playerSize.y / 2.0f };
            float dist_sq = (player_center - source.GetPosition()).LengthSq();

            // 3. 이전에 찾은 공급원보다 더 가깝다면, '가장 가까운 공급원'으로 기록합니다.
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
    }

    bool is_near_charger = (closest_source != nullptr);

    // 4. 가장 가까운 공급원이 있을 때만 충전 로직이 동작하도록 합니다.
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_charger, false, dt); // isDashing 대신 공격(false) 전달

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);
        Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", result.delta);
    }
}