#pragma once
#include "../Engine/Vec2.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include <vector>

class PulseManager
{
public:

    void Update(Math::Vec2 playerHitboxCenter, Math::Vec2 playerHitboxSize,
        Player& player, std::vector<PulseSource>& pulseSources,
        bool isPressingE, double dt);
};