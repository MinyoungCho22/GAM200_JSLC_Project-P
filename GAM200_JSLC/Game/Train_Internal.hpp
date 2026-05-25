// Train_Internal.hpp
// Shared static helpers and constants used across Train_*.cpp files.
// Do NOT include this outside of Train_*.cpp implementations.

#pragma once
#include "Train.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// Shared constants (file-scope static — each TU gets its own copy, which is fine)
// ---------------------------------------------------------------------------
static constexpr float ASSUMED_IMG_HEIGHT = 1080.0f;
static constexpr float kTrainFlatbedDeckTopLocalY = ASSUMED_IMG_HEIGHT - 804.f;
static constexpr float kRailWalkSurfaceFractionOfTileH = 0.08f;
static constexpr const char* kTrainDroneTexturePath = "Asset/Drone.png";

// Car4 transport constants — used in Train_Car4.cpp, Train_Init.cpp, Train_Draw.cpp
static constexpr int   kCarTransportCount             = 6;
static constexpr int   kCarTransportHoverDroneCount   = 14;

struct TrainCarDronePixel { float x, yTop; };
static const TrainCarDronePixel kCarTransportDronePixels[kCarTransportHoverDroneCount] = {
    { 620.f, 52.f },  { 1380.f, 50.f }, { 2100.f, 54.f }, { 2850.f, 48.f },
    { 700.f, 258.f }, { 1480.f, 266.f }, { 2280.f, 252.f }, { 3080.f, 260.f },
    { 330.f, 122.f }, { 315.f, 292.f },  { 360.f, 532.f },
    { 3380.f, 118.f }, { 3420.f, 288.f }, { 3360.f, 528.f },
};

// ---------------------------------------------------------------------------
// Static helper functions shared by multiple Train_*.cpp files
// ---------------------------------------------------------------------------
// 드론 시각 크기를 전투용 비율로 축소함 (각 TU에서 독립 호출함)
static void ScaleTrainCombatDrone(Drone& d)
{
    Train::ApplyCombatDroneVisualScale(d);
}

// 로봇 시각 크기를 드론과 동일한 전투 비율로 축소함
static void ScaleTrainCombatRobot(Robot& r)
{
    const Math::Vec2 s = r.GetSize();
    r.SetSize({ s.x * Train::kCombatDroneVisualScale, s.y * Train::kCombatDroneVisualScale });
}

// 각도를 [-π, π] 범위로 정규화함
static float WrapAnglePi(float a)
{
    while (a > 3.14159265359f) a -= 6.28318530718f;
    while (a < -3.14159265359f) a += 6.28318530718f;
    return a;
}

// 픽셀 좌표(이미지 좌상 기준)를 Y축 반전한 로컬 중심 좌표의 TrainHitbox로 변환함
static Train::TrainHitbox MakeHitbox(float carOffset, float px, float py, float pw, float ph,
                                     bool collision = true,
                                     Train::TrainHitboxKind kind = Train::TrainHitboxKind::Solid)
{
    float cx = carOffset + px + pw * 0.5f;
    float cy = ASSUMED_IMG_HEIGHT - py - ph * 0.5f;
    return { {cx, cy}, {pw, ph}, collision, kind };
}

// Third_Third 자동차 영역 윤곽 박스를 추가함 — collision=false이면 플레이어가 통과함
static void AppendCarSilhouette(std::vector<Train::TrainHitbox>& out, float carOffset,
                                float px, float py, float pw, float ph, bool collision)
{
    out.push_back(MakeHitbox(carOffset, px, py, pw, ph, collision, Train::TrainHitboxKind::Solid));
}

// 히트박스 크기가 가로로 긴 얇은 슬랩인지 확인함 (발판 충돌 특수 처리 여부 판별용)
static bool IsThinHorizontalTrainSlab(const Math::Vec2& obsSize)
{
    return obsSize.y <= 72.0f && obsSize.x >= obsSize.y * 3.0f;
}

// 히트박스가 열차 캐리 밴드(Y 범위·X 범위) 안에 있는지 확인함
static bool HitboxInTrainCarryBand(float trainWorldLeft, float totalTrainWidth,
                                   const Math::Vec2& hbCenter, const Math::Vec2& hbHalf)
{
    constexpr float margin = 56.0f;
    const float l = trainWorldLeft - margin;
    const float r = trainWorldLeft + totalTrainWidth + margin;
    if (hbCenter.x + hbHalf.x < l || hbCenter.x - hbHalf.x > r)
        return false;
    if (hbCenter.y + hbHalf.y < Train::MIN_Y + 24.0f)
        return false;
    if (hbCenter.y - hbHalf.y > Train::MIN_Y + Train::HEIGHT + 40.0f)
        return false;
    return true;
}

static constexpr float kTrainFlatbedDeckSlabCenterLocalY = kTrainFlatbedDeckTopLocalY - 45.f * 0.5f;

// 히트박스가 열차 평판 덱 슬랩(얇은 가로 판, 덱 높이 근처)인지 확인함
static bool IsTrainFlatbedDeckSlab(const Train::TrainHitbox& hb)
{
    if (!hb.collision || hb.kind != Train::TrainHitboxKind::Solid)
        return false;
    if (!IsThinHorizontalTrainSlab(hb.size))
        return false;
    return std::fabs(hb.localCenter.y - kTrainFlatbedDeckSlabCenterLocalY) < 2.0f;
}

// 플레이어 히트박스가 X축으로 열차 덱 슬랩과 겹치는지 확인함
static bool PlayerHitboxOverlapsAnyTrainDeckX(float trainWorldLeft, Math::Vec2 hbCenter, Math::Vec2 halfHb,
                                              const std::vector<Train::TrainHitbox>& boxes)
{
    const float pMinX = hbCenter.x - halfHb.x;
    const float pMaxX = hbCenter.x + halfHb.x;
    for (const auto& hb : boxes)
    {
        if (!IsTrainFlatbedDeckSlab(hb))
            continue;
        const float L = trainWorldLeft + hb.localCenter.x - hb.size.x * 0.5f;
        const float R = trainWorldLeft + hb.localCenter.x + hb.size.x * 0.5f;
        if (pMaxX > L && pMinX < R)
            return true;
    }
    return false;
}

// 열차 칸 사이 갭 위에 플레이어가 있는지 확인함
// — 캐리 밴드 안이고 덱 X 범위와 겹치지 않을 때 낙하 트리거로 사용함
static bool PlayerStandsInTrainCarGap(float trainWorldLeft, float totalTrainWidth, Math::Vec2 hbCenter,
                                      Math::Vec2 halfHb, const std::vector<Train::TrainHitbox>& boxes,
                                      const Player& player)
{
    if (player.IsGodMode() || player.IsDead())
        return false;
    if (!HitboxInTrainCarryBand(trainWorldLeft, totalTrainWidth, hbCenter, halfHb))
        return false;
    if (PlayerHitboxOverlapsAnyTrainDeckX(trainWorldLeft, hbCenter, halfHb, boxes))
        return false;

    const float feetY = hbCenter.y - halfHb.y;
    const float deckTop = Train::MIN_Y + kTrainFlatbedDeckTopLocalY;
    const float vy      = player.GetVelocity().y;

    if (vy > 42.f && feetY > deckTop + 48.f)
        return false;
    if (feetY < deckTop - 260.f)
        return false;
    if (feetY > deckTop + 135.f)
        return false;
    return true;
}
