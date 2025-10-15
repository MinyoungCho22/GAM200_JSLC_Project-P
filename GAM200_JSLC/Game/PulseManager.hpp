#pragma once
#include <vector>

class Player;
class PulseSource;

class PulseManager
{
public:
    // [수정] double dt 인자 추가
    void Update(Player& player, std::vector<PulseSource>& sources, bool is_interact_key_pressed, double dt);
};