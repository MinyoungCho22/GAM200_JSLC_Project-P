//MapObjectTypes.hpp

#pragma once

#include "../Engine/Vec2.hpp"
#include <string>
#include <vector>

struct SpriteRectConfig
{
    Math::Vec2 topLeft{};
    Math::Vec2 size{};
    std::string spritePath{};
    Math::Vec2 fallbackSize{};
    float hitboxMargin = 0.0f;
    int sharedPulseGroup = 0;
    bool gaugeAnchor = true;
    /// If >= 0 and `shared_pulse_group` matches an earlier entry, top-left X is placed
    /// after that entry's resolved width plus this gap (map raw coords).
    float leaderRightGap = -1.0f;
    /// Added to follower X when `leader_right_gap` layout applies (map raw coords).
    float layoutOffsetX = 0.0f;
};

struct HallwayObjectConfig
{
    std::vector<SpriteRectConfig> pulseSources{};
    std::vector<SpriteRectConfig> hidingSpots{};
    SpriteRectConfig obstacle{};
};

struct RooftopObjectConfig
{
    SpriteRectConfig hole{};
    std::vector<SpriteRectConfig> pulseSources{};
    SpriteRectConfig liftButton{};
};

struct RoomObjectConfig
{
    std::vector<SpriteRectConfig> pulseSources{};
    SpriteRectConfig blind{};
};

struct UndergroundObjectConfig
{
    std::vector<SpriteRectConfig> obstacles{};
    std::vector<SpriteRectConfig> pulseSources{};
    std::vector<SpriteRectConfig> ramps{};
    /// Decorative sprites (e.g. top light strip); same `top_left` / `size` / `sprite` as obstacles.
    std::vector<SpriteRectConfig> lights{};
    std::vector<Math::Vec2> robotSpawns{};
};

struct TrainObjectConfig
{
    std::vector<SpriteRectConfig> obstacles{};
    std::vector<SpriteRectConfig> pulseSources{};
};

struct MapObjectConfigData
{
    RoomObjectConfig room{};
    HallwayObjectConfig hallway{};
    RooftopObjectConfig rooftop{};
    UndergroundObjectConfig underground{};
    TrainObjectConfig train{};
};

