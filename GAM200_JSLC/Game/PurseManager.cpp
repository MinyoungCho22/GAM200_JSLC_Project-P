#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PurseCore.hpp"

void PulseManager::Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed)
{
    bool is_near_any_charger = false;
    PulseSource* nearby_source = nullptr;

    // 1. ��� �޽� ���޿��� ��ȸ�ϸ� �÷��̾�� �浹�ϴ��� Ȯ���մϴ�.
    for (auto& source : sources)
    {
        if (!source.HasPulse()) continue; // �޽��� ���� �ҽ��� �ǳʶݴϴ�.

        Math::Vec2 playerPos = player.GetPosition();
        Math::Vec2 playerSize = player.GetSize();
        Math::Vec2 sourcePos = source.GetPosition();
        Math::Vec2 sourceSize = source.GetSize();

        // ������ AABB �浹 �˻�
        if (playerPos.x < sourcePos.x + sourceSize.x &&
            playerPos.x + playerSize.x > sourcePos.x &&
            playerPos.y < sourcePos.y + sourceSize.y &&
            playerPos.y + playerSize.y > sourcePos.y)
        {
            is_near_any_charger = true;
            nearby_source = &source;
            break; // �ϳ��� ã�Ƶ� ������ ����ϴ�.
        }
    }

    // 2. �÷��̾��� PulseCore�� ������Ʈ(tick)�ϰ�, ����� �޽��ϴ�.
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, false, is_near_any_charger, false);

    // 3. ���� ������ �Ͼ�ٸ�, �޽� ���޿��� �������� ������ ���ҽ�ŵ�ϴ�.
    if (result.charged && nearby_source != nullptr)
    {
        nearby_source->Drain(result.delta);
    }
}