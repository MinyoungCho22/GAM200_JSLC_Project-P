#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp"

void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed, double dt)
{
    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;

    // GameplayState와 동일한 방식으로 플레이어의 중심과 히트박스 크기를 먼저 계산
    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();
    Math::Vec2 playerCenter = { playerPos.x + 60.0f, playerPos.y + playerSize.y / 1.1f };
    Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f };

    // 계산된 중심과 크기를 바탕으로 플레이어의 히트박스(AABB)를 구합니다.
    float playerMinX = playerCenter.x - playerHitboxSize.x / 2.0f;
    float playerMaxX = playerCenter.x + playerHitboxSize.x / 2.0f;
    float playerMinY = playerCenter.y - playerHitboxSize.y / 2.0f;
    float playerMaxY = playerCenter.y + playerHitboxSize.y / 2.0f;

    // 1. 플레이어와 충돌하는 모든 공급원을 확인합니다.
    for (auto& source : sources)
    {
        if (!source.HasPulse()) continue;

        // PulseSource의 히트박스 계산
        Math::Vec2 sourcePos = source.GetPosition();
        Math::Vec2 sourceSize = source.GetSize();
        float sourceMinX = sourcePos.x - sourceSize.x / 2.0f;
        float sourceMaxX = sourcePos.x + sourceSize.x / 2.0f;
        float sourceMinY = sourcePos.y - sourceSize.y / 2.0f;
        float sourceMaxY = sourcePos.y + sourceSize.y / 2.0f;

        // 수정된 플레이어 히트박스와 PulseSource의 충돌을 검사
        if (playerMaxX > sourceMinX && playerMinX < sourceMaxX &&
            playerMaxY > sourceMinY && playerMinY < sourceMaxY)
        {
            // 2. 충돌이 감지되면, 플레이어와의 거리를 계산합니다.
            // [수정] 거리 계산 시에도 일관되게 새로 정의한 playerCenter를 사용
            float dist_sq = (playerCenter - source.GetPosition()).LengthSq();

            // 3. 이전에 찾은 공급원보다 더 가깝다면, '가장 가까운 공급원'으로 기록
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
    }

    bool is_near_charger = (closest_source != nullptr);

    // 4. 가장 가까운 공급원이 있을 때만 충전 로직이 동작
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_charger, false, dt);

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);
        Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", result.delta);
    }
}