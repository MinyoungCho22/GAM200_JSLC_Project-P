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

    // GameplayState�� ������ ������� �÷��̾��� �߽ɰ� ��Ʈ�ڽ� ũ�⸦ ���� ���
    Math::Vec2 playerPos = player.GetPosition();
    Math::Vec2 playerSize = player.GetSize();
    Math::Vec2 playerCenter = { playerPos.x + 60.0f, playerPos.y + playerSize.y / 1.1f };
    Math::Vec2 playerHitboxSize = { playerSize.x * 0.4f, playerSize.y * 0.8f };

    // ���� �߽ɰ� ũ�⸦ �������� �÷��̾��� ��Ʈ�ڽ�(AABB)�� ���մϴ�.
    float playerMinX = playerCenter.x - playerHitboxSize.x / 2.0f;
    float playerMaxX = playerCenter.x + playerHitboxSize.x / 2.0f;
    float playerMinY = playerCenter.y - playerHitboxSize.y / 2.0f;
    float playerMaxY = playerCenter.y + playerHitboxSize.y / 2.0f;

    // 1. �÷��̾�� �浹�ϴ� ��� ���޿��� Ȯ���մϴ�.
    for (auto& source : sources)
    {
        if (!source.HasPulse()) continue;

        // PulseSource�� ��Ʈ�ڽ� ���
        Math::Vec2 sourcePos = source.GetPosition();
        Math::Vec2 sourceSize = source.GetSize();
        float sourceMinX = sourcePos.x - sourceSize.x / 2.0f;
        float sourceMaxX = sourcePos.x + sourceSize.x / 2.0f;
        float sourceMinY = sourcePos.y - sourceSize.y / 2.0f;
        float sourceMaxY = sourcePos.y + sourceSize.y / 2.0f;

        // ������ �÷��̾� ��Ʈ�ڽ��� PulseSource�� �浹�� �˻�
        if (playerMaxX > sourceMinX && playerMinX < sourceMaxX &&
            playerMaxY > sourceMinY && playerMinY < sourceMaxY)
        {
            // 2. �浹�� �����Ǹ�, �÷��̾���� �Ÿ��� ����մϴ�.
            // [����] �Ÿ� ��� �ÿ��� �ϰ��ǰ� ���� ������ playerCenter�� ���
            float dist_sq = (playerCenter - source.GetPosition()).LengthSq();

            // 3. ������ ã�� ���޿����� �� �����ٸ�, '���� ����� ���޿�'���� ���
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
    }

    bool is_near_charger = (closest_source != nullptr);

    // 4. ���� ����� ���޿��� ���� ���� ���� ������ ����
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_charger, false, dt);

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);
        Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", result.delta);
    }
}