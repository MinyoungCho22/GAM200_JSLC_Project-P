#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp" // Vec2�� �Ÿ� ����� ���� �߰�

void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed, double dt)
{
    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f; // ���� ����� �Ÿ��� �����ϱ� ���� ���� (���������� ��)

    // 1. �÷��̾�� �浹�ϴ� ��� ���޿��� Ȯ���մϴ�.
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
            // 2. �浹�� �����Ǹ�, �÷��̾���� �Ÿ��� ����մϴ�.
            Math::Vec2 player_center = { playerPos.x, playerPos.y + playerSize.y / 2.0f };
            float dist_sq = (player_center - source.GetPosition()).LengthSq();

            // 3. ������ ã�� ���޿����� �� �����ٸ�, '���� ����� ���޿�'���� ����մϴ�.
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
    }

    bool is_near_charger = (closest_source != nullptr);

    // 4. ���� ����� ���޿��� ���� ���� ���� ������ �����ϵ��� �մϴ�.
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_charger, false, dt); // isDashing ��� ����(false) ����

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);
        Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", result.delta);
    }
}