#include "PulseManager.hpp"
#include "Player.hpp"          // Player::GetPulseCore()를 위해 필요
#include "PulseSource.hpp"
#include "../Game/PulseCore.hpp"   // PulseTickResult를 위해 필요
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp"
#include "../Engine/Collision.hpp" // Collision::CheckAABB를 위해 필요

// ✅ [수정] 함수 시그니처를 .hpp 파일과 일치시킵니다.
void PulseManager::Update(Math::Vec2 playerHitboxCenter, Math::Vec2 playerHitboxSize,
    Player& player, std::vector<PulseSource>& sources,
    bool is_interact_key_pressed, double dt)
{
    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;

    // ✅ [제거] GameplayState에서 이미 계산했으므로, 
    // PulseManager 내부에서 playerCenter와 playerHitboxSize를 중복 계산하는 로직을 삭제합니다.

    // 1. 플레이어와 충돌하는 모든 공급원을 확인합니다.
    for (auto& source : sources)
    {
        if (!source.HasPulse()) continue;

        // ✅ [수정] GameplayState에서 전달받은 playerHitboxCenter와 playerHitboxSize를 사용합니다.
        if (Collision::CheckAABB(playerHitboxCenter, playerHitboxSize, source.GetPosition(), source.GetSize()))
        {
            // 2. 충돌이 감지되면, 플레이어와의 거리를 계산합니다.
            // ✅ [수정] 중심점도 전달받은 playerHitboxCenter를 사용합니다.
            float dist_sq = (playerHitboxCenter - source.GetPosition()).LengthSq();

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