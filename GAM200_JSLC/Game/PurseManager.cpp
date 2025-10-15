#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"

void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed)
{
    bool is_near_any_charger = false;
    PulseSource* nearby_source = nullptr;

    // 1. 모든 펄스 공급원을 순회하며 플레이어와 충돌하는지 확인합니다.
    for (auto& source : sources)
    {
        if (!source.HasPulse()) continue; // 펄스가 없는 소스는 건너뜁니다.

        Math::Vec2 playerPos = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        Math::Vec2 sourcePos = source.GetPosition();
        Math::Vec2 sourceSize = source.GetSize();

        // 간단한 AABB 충돌 검사
        if (playerPos.x < sourcePos.x + sourceSize.x &&
            playerPos.x + playerSize.x > sourcePos.x &&
            playerPos.y < sourcePos.y + sourceSize.y &&
            playerPos.y + playerSize.y > sourcePos.y)
        {
            is_near_any_charger = true;
            nearby_source = &source;
            break; // 하나만 찾아도 루프를 멈춥니다.
        }
    }

    // 2. 플레이어의 PulseCore를 업데이트(tick)하고, 결과를 받습니다.
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, false, is_near_any_charger, false);

    // 3. 만약 충전이 일어났다면, 펄스 공급원의 에너지를 실제로 감소시킵니다.
    if (result.charged && nearby_source != nullptr)
    {
        nearby_source->Drain(result.delta);
    }
}