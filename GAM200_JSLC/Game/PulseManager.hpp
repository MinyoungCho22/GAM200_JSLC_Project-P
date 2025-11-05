#pragma once
#include "../Engine/Vec2.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include <vector>

class PulseManager
{
public:
    // ✅ Update 함수의 인자를 player 대신 playerHitbox...로 변경
    void Update(Math::Vec2 playerHitboxCenter, Math::Vec2 playerHitboxSize,
        Player& player, std::vector<PulseSource>& pulseSources,
        bool isPressingE, double dt);
};