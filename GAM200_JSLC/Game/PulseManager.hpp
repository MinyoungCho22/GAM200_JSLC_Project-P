#pragma once
#include <vector>

// 전방 선언
class Player;
class PulseSource;

class PulseManager
{
public:
    void Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed);
};