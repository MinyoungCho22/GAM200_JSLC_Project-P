#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"
#include "../Engine/Logger.hpp"

// [����] dt ���ڸ� ������ tick �Լ��� ������� �ʴ� ���ο� ����
void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed, double dt)
{
    // 1. ��� �޽� ���޿��� ��ȸ�ϸ� �÷��̾�� �浹�ϴ��� Ȯ���մϴ�.
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

            // 2. �浹 ���̰� EŰ�� ������ �ִٸ�
            if (is_interact_key_pressed)
            {
                // 3. ���޿����� �� ���� ����ϰ�, ������ �� ���� �޾ƿɴϴ�.
                float drain_amount_per_frame = player.GetPulseCore().getConfig().chargeRatePerSecond * static_cast<float>(dt);
                float drained_amount = source.Drain(drain_amount_per_frame);

                // 4. ������ �� �縸ŭ �÷��̾��� �޽��� ä��ϴ�.
                if (drained_amount > 0)
                {
                    player.GetPulseCore().getPulse().add(drained_amount);
                    Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", drained_amount);
                }
            }
            break; // �� ���� �ϳ��� ���޿����� ��ȣ�ۿ�
        }
    }
}