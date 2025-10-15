#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"
#include "../Engine/Logger.hpp"

// [수정] dt 인자를 받지만 tick 함수는 사용하지 않는 새로운 로직
void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed, double dt)
{
    // 1. 모든 펄스 공급원을 순회하며 플레이어와 충돌하는지 확인합니다.
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
            Logger::Instance().Log(Logger::Severity::Debug, "Collision Detected with a pulse source!");

            // 2. 충돌 중이고 E키를 누르고 있다면
            if (is_interact_key_pressed)
            {
                // 3. 공급원에서 뺄 양을 계산하고, 실제로 뺀 양을 받아옵니다.
                float drain_amount_per_frame = player.GetPulseCore().getConfig().chargeRatePerSecond * static_cast<float>(dt);
                float drained_amount = source.Drain(drain_amount_per_frame);

                // 4. 실제로 뺀 양만큼 플레이어의 펄스를 채웁니다.
                if (drained_amount > 0)
                {
                    player.GetPulseCore().getPulse().add(drained_amount);
                    Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", drained_amount);
                }
            }
            break; // 한 번에 하나의 공급원과만 상호작용
        }
    }
}