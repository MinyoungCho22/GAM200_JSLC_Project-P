#pragma once
#include <vector>

// ���� ����
class Player;
class PulseSource;

class PulseManager
{
public:
    void Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed);
};