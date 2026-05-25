// Train_Helpers.cpp - Query helpers, robot AI, clamp utilities, motion helpers

#include "Train_Internal.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "MapObjectConfig.hpp"
#include "Robot.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// Q 펄스 범위 내 열차 로봇 모두에게 방향성 넉백과 고정 데미지를 적용함
void Train::ApplyPulseToTrainRobots(Math::Vec2 pulseWorldCenter, float radius)
{
    constexpr float kDamage            = 14.f;
    constexpr float kImpulseCenter     = 900.f;
    constexpr float kImpulseEdge       = 400.f;
    constexpr float kRobotImpulseScale = 0.48f;
    constexpr float kLift              = 135.f;

    const float rSq = radius * radius;
    for (auto& robot : m_robots)
    {
        if (robot.IsDead())
            continue;
        const Math::Vec2 pos = robot.GetPosition();
        Math::Vec2       d   = pos - pulseWorldCenter;
        const float      dSq = d.LengthSq();
        if (dSq > rSq)
            continue;
        const float dist = std::sqrt(dSq);
        float       t    = (dist < 0.1f) ? 1.f : (1.f - dist / (radius + 1.f));
        t                = (std::max)(0.f, t);
        const float      impulseMag = t * (kImpulseCenter - kImpulseEdge) + kImpulseEdge;
        const Math::Vec2 dir        = (dist > 0.1f) ? (d * (1.f / dist)) : Math::Vec2{ 1.f, 0.f };
        const Math::Vec2 impulse    = dir * (impulseMag * kRobotImpulseScale) + Math::Vec2{ 0.f, kLift };
        robot.ApplyPulseImpact(impulse, kDamage);
    }
}

// 지정 칸(1~5)의 로컬 X 왼쪽 경계 오프셋을 반환함 (칸 6 이상이면 전체 열차 폭을 반환함)
float Train::GetTrainCarLocalLeftEdge(int carIndex1To5) const
{
    switch (carIndex1To5)
    {
    case 1: return 0.f;
    case 2: return m_car1Width;
    case 3: return m_car1Width + m_car2Width;
    case 4: return m_car1Width + m_car2Width + m_car3Width;
    case 5: return m_car1Width + m_car2Width + m_car3Width + m_car4Width;
    case 6: return m_totalTrainWidth;
    default: return m_totalTrainWidth;
    }
}

// 플레이어 히트박스 중심 X로 현재 탑승 칸 번호(1~5)를 반환함 (열차 밖이면 0)
int Train::GetPlayerTrainCarIndex(Math::Vec2 worldHbCenter) const
{
    const float lx = worldHbCenter.x - MIN_X - m_trainOffset;
    if (lx < 0.f || lx >= m_totalTrainWidth)
        return 0;
    if (lx < m_car1Width)
        return 1;
    if (lx < m_car1Width + m_car2Width)
        return 2;
    if (lx < m_car1Width + m_car2Width + m_car3Width)
        return 3;
    if (lx < m_car1Width + m_car2Width + m_car3Width + m_car4Width)
        return 4;
    return 5;
}

// 지정 칸(1~5)의 월드 X 중심 좌표를 반환함 (범위 밖이면 열차 전체 중앙을 반환함)
float Train::GetTrainCarCenterWorldX(int carIndex1To5) const
{
    if (carIndex1To5 < 1 || carIndex1To5 > 5)
        return MIN_X + m_trainOffset + m_totalTrainWidth * 0.5f;
    const float l = GetTrainCarLocalLeftEdge(carIndex1To5);
    float       w = m_car1Width;
    if (carIndex1To5 == 2)
        w = m_car2Width;
    else if (carIndex1To5 == 3)
        w = m_car3Width;
    else if (carIndex1To5 == 4)
        w = m_car4Width;
    else if (carIndex1To5 == 5)
        w = m_car5Width;
    return MIN_X + m_trainOffset + l + w * 0.5f;
}

// 해당 칸의 드론과 로봇이 모두 처치됐으면 true를 반환함
bool Train::IsTrainCarCombatCleared(int car1To5) const
{
    if (car1To5 < 1 || car1To5 > 5)
        return true;

    if (m_droneManager)
    {
        for (const auto& d : m_droneManager->GetDrones())
            if (!d.IsDead() && d.GetTrainCarSegment() == car1To5)
                return false;
    }
    if (car1To5 == 3 && m_sirenDroneManager)
    {
        for (const auto& d : m_sirenDroneManager->GetDrones())
            if (!d.IsDead())
                return false;
    }
    if (car1To5 == 4 && m_carTransportDroneManager)
    {
        for (const auto& d : m_carTransportDroneManager->GetDrones())
            if (!d.IsDead())
                return false;
    }
    for (const auto& r : m_robots)
        if (!r.IsDead() && r.GetTrainCarSegment() == car1To5)
            return false;
    return true;
}

// 미클리어 칸으로 인한 플레이어 진행 한계 X 좌표를 반환함 (치트 활성 시 열차 끝까지 반환함)
float Train::GetTrainCombatAdvanceCapWorldX() const
{
    if (m_trainCheatCarUnlock)
        return MIN_X + m_trainOffset + m_totalTrainWidth - 85.f;

    const float tailGuard = MIN_X + m_trainOffset + m_totalTrainWidth - 85.f;

    // Cars 1–3: single zone — move freely until all three are cleared, then cap at car 4.
    const bool zone123Cleared =
        IsTrainCarCombatCleared(1) && IsTrainCarCombatCleared(2) && IsTrainCarCombatCleared(3);
    if (!zone123Cleared)
        return std::min(MIN_X + m_trainOffset + GetTrainCarLocalLeftEdge(4) - 38.f, tailGuard);
    if (!IsTrainCarCombatCleared(4))
        return std::min(MIN_X + m_trainOffset + GetTrainCarLocalLeftEdge(5) - 38.f, tailGuard);

    return tailGuard;
}

// Car5 첫 진입 시 밸브 조작 힌트 배너 문자열을 반환함 (타이머 만료 시 빈 문자열 반환함)
std::string Train::GetCar5ValveHintBannerText() const
{
    if (m_car5ValveHintTimer <= 0.0f)
        return {};
    return "Last car reached. Spin the valve to the right — flood the deck and wash the robots away!";
}

// 카메라 흔들림 요청값을 누적해 가장 큰 값으로 예약함
void Train::RequestTrainCameraShake(float maxPixelOffset)
{
    m_pendingTrainCameraShakePx = std::max(m_pendingTrainCameraShakePx, maxPixelOffset);
}

// 예약된 카메라 흔들림 픽셀 값을 꺼내고 내부 값을 0으로 초기화함
float Train::ConsumeTrainCameraShakeRequest()
{
    const float t                  = m_pendingTrainCameraShakePx;
    m_pendingTrainCameraShakePx = 0.f;
    return t;
}

// 살아있는 열차 로봇 각각의 경계 감지 경보 아이콘을 그림
void Train::DrawRobotTrainAlerts(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
            robot.DrawTrainDetectAlert(colorShader, debugRenderer);
    }
}

// 덱 패트롤 로봇의 공중→착지 전환 시 카메라 흔들림을 발생시킴 (중복 흔들림 방지 포함)
void Train::TryDeckPatrolRobotJumpAndLandingShake(Robot& r, size_t robotIndex, float dt,
                                                   const std::vector<ObstacleInfo>& tallCarObs, int playerTrainCar)
{
    if (r.IsDead() || !r.IsTrainDeckPatrol() || robotIndex >= m_trainDeckRobotWasAirborne.size())
        return;
    if (robotIndex >= m_trainDeckRobotJumpPrepT.size())
        return;

    const RobotState stJump = r.GetState();
    if (stJump == RobotState::Windup || stJump == RobotState::Attack)
    {
        m_trainDeckRobotJumpPrepT[robotIndex] = 0.f;
        return;
    }

    const bool airborne = !r.IsOnGround() || r.GetVelocity().y > 95.f;
    if (!airborne && m_trainDeckRobotWasAirborne[robotIndex])
    {
        if (playerTrainCar == r.GetTrainCarSegment() && robotIndex < m_trainDeckRobotUsedLandingShake.size()
            && !m_trainDeckRobotUsedLandingShake[robotIndex])
        {
            RequestTrainCameraShake(22.f);
            m_trainDeckRobotUsedLandingShake[robotIndex] = true;
        }
    }
    m_trainDeckRobotWasAirborne[robotIndex] = airborne;

    if (!r.IsOnGround())
    {
        m_trainDeckRobotJumpPrepT[robotIndex] = 0.f;
        return;
    }
    if (r.GetVelocity().y > 25.f)
        return;

    // No deck vault/jump (match Underground robots; avoids Car2 purple container snagging / flicker).
    (void)tallCarObs;
}

// Car4(자동차 운반 칸) 로봇이 밸브 앞쪽으로 넘어가지 못하도록 X 위치를 제한함
void Train::ClampCarSegment4RobotsBeforeValve()
{
    const float valveX = MIN_X + m_trainOffset + m_valveLocalCenter.x;
    constexpr float kMargin = 168.f;
    const float capX = valveX - kMargin;

    for (auto& r : m_robots)
    {
        if (r.IsDead() || r.GetTrainCarSegment() != 4)
            continue;
        Math::Vec2 p = r.GetPosition();
        if (p.x <= capX)
            continue;
        p.x = capX;
        r.SetPosition(p);
        Math::Vec2 v = r.GetVelocity();
        if (v.x > 0.f)
            v.x = 0.f;
        r.SetVelocity(v);
    }
}

// Car5 로봇이 밸브 왼쪽(작은 X) 방향으로 이동하지 못하도록 위치를 제한함
void Train::ClampCar5RobotsEastOfValve()
{
    // 밸브 중심보다 왼쪽(작은 X)으로 못 가게 — 마지막 칸 로봇은 밸브 오른쪽 덱에만
    const float     valveCx = MIN_X + m_trainOffset + m_valveLocalCenter.x;
    constexpr float kPastValve = 188.f;
    const float     minRobotCx = valveCx + kPastValve;

    for (auto& r : m_robots)
    {
        if (r.IsDead() || r.GetTrainCarSegment() != 5)
            continue;
        Math::Vec2 p = r.GetPosition();
        if (p.x >= minRobotCx)
            continue;
        p.x = minRobotCx;
        r.SetPosition(p);
        Math::Vec2 v = r.GetVelocity();
        if (v.x < 0.f)
            v.x = 0.f;
        r.SetVelocity(v);
    }
}

// 덱 위 패트롤 로봇 전체의 AI(시야·추격·공격·칸 이동 제한)를 매 프레임 갱신함
void Train::UpdateTrainDeckPatrolRobots(float dt, Player& player, Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize)
{
    (void)playerHitboxSize;
    if (m_robots.empty())
        return;

    const float tl   = MIN_X + m_trainOffset;
    const int   pcar = GetPlayerTrainCarIndex(playerHbCenter);

    for (size_t ri = 0; ri < m_robots.size(); ++ri)
    {
        Robot& r = m_robots[ri];
        if (r.IsDead() || !r.IsTrainDeckPatrol())
            continue;

        const int seg = r.GetTrainCarSegment();
        if (seg != 1 && seg != 2 && seg != 4 && seg != 5)
            continue;

        float carW = m_car1Width;
        if (seg == 2)
            carW = m_car2Width;
        else if (seg == 4)
            carW = m_car4Width;
        else if (seg == 5)
            carW = m_car5Width;

        const float carLocalL = GetTrainCarLocalLeftEdge(seg);
        const float carWorldL = tl + carLocalL;
        const float carWorldR = carWorldL + carW;

        r.SetAllowTrainCombatVsPlayer(pcar == seg);

        std::vector<ObstacleInfo> carTall;
        carTall.reserve(8);
        const float minHbH = (seg == 5 || seg == 1 || seg == 2) ? 68.f : 100.f;
        for (const auto& hb : m_trainHitboxes)
        {
            if (!hb.collision)
                continue;
            if (hb.kind == TrainHitboxKind::JumpThroughPipe)
                continue;
            if (hb.size.y < minHbH)
                continue;
            Math::Vec2 world = { tl + hb.localCenter.x, MIN_Y + hb.localCenter.y };
            const float wxL = world.x - hb.size.x * 0.5f;
            const float wxR = world.x + hb.size.x * 0.5f;
            if (wxR < carWorldL || wxL > carWorldR)
                continue;
            carTall.push_back({ world, hb.size });
        }

        r.SetUsePatrolWorldClamp(true);
        if (seg == 4)
        {
            const float valveX = MIN_X + m_trainOffset + m_valveLocalCenter.x;
            const float capX   = valveX - 160.f;
            r.SetPatrolWorldClamp(carWorldL + 90.f, std::min(carWorldR - 90.f, capX));
        }
        else if (seg == 5)
        {
            const float valveCx    = MIN_X + m_trainOffset + m_valveLocalCenter.x;
            constexpr float kEastV = 188.f;
            const float minPatrolX = valveCx + kEastV;
            r.SetPatrolWorldClamp(std::max(carWorldL + 90.f, minPatrolX), carWorldR - 90.f);
        }
        else
            r.SetPatrolWorldClamp(carWorldL + 90.f, carWorldR - 90.f);

        r.Update(dt, player, carTall, carWorldL + 55.f, carWorldR - 55.f);

        if (seg == 2 && m_car2PurpleHbValid && !r.IsDead())
        {
            Math::Vec2       rp = r.GetPosition();
            const Math::Vec2 rs = r.GetSize();
            const Math::Vec2 pc = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };
            const Math::Vec2 psz = m_car2PurpleHb.size;
            if (Collision::CheckAABB(rp, rs, pc, psz))
            {
                const float rL = rp.x - rs.x * 0.5f, rR = rp.x + rs.x * 0.5f;
                const float rB = rp.y - rs.y * 0.5f, rT = rp.y + rs.y * 0.5f;
                const float pL = pc.x - psz.x * 0.5f, pR = pc.x + psz.x * 0.5f;
                const float pB = pc.y - psz.y * 0.5f, pT = pc.y + psz.y * 0.5f;
                const float overlapX = std::min(rR, pR) - std::max(rL, pL);
                const float overlapY = std::min(rT, pT) - std::max(rB, pB);
                constexpr float kSepPad = 8.f;
                if (overlapX < overlapY)
                    rp.x += (rp.x < pc.x) ? -(overlapX + kSepPad) : (overlapX + kSepPad);
                else
                    rp.y += (rp.y < pc.y) ? -(overlapY + kSepPad) : (overlapY + kSepPad);
                r.SetPosition(rp);
            }
        }

        TryDeckPatrolRobotJumpAndLandingShake(r, ri, static_cast<float>(dt), carTall, pcar);

        if (seg == 5 && m_valvePressureT > 0.10f && !r.IsDead())
        {
            Math::Vec2 p = r.GetPosition();
            p.x += 520.f * dt;
            const float maxX = carWorldR - 55.f;
            if (p.x > maxX)
                p.x = maxX;
            r.SetPosition(p);
            Math::Vec2 v = r.GetVelocity();
            if (v.x < 180.f)
                v.x = 180.f;
            r.SetVelocity(v);
        }
    }

    ClampCarSegment4RobotsBeforeValve();
    ClampCar5RobotsEastOfValve();
}


// 열차 이동량(deltaTrainX)을 전투·사이렌·자동차 운반 드론 및 로봇 위치에 반영함
void Train::ApplyTrainMotionToDronesAndRobots(float deltaTrainX)
{
    if (deltaTrainX == 0.0f)
        return;

    if (m_droneManager)
    {
        for (auto& d : m_droneManager->GetDrones())
            d.SetPosition(d.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
    }

    if (m_sirenDroneManager)
    {
        for (auto& d : m_sirenDroneManager->GetDrones())
            d.SetPosition(d.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
    }

    if (m_carTransportDroneManager)
    {
        for (auto& d : m_carTransportDroneManager->GetDrones())
            d.SetPosition(d.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
    }

    for (auto& r : m_robots)
        r.SetPosition(r.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
}


// 플레이어가 Car5 덱 발판 위에 있는지 반환함 (히트박스 유효하지 않으면 false 반환함)
bool Train::IsPlayerOnCar5Deck(Math::Vec2 hbCenter, Math::Vec2 hbSize) const
{
    if (!m_car5DeckHbValid)
        return false;

    const float        tl       = MIN_X + m_trainOffset;
    Math::Vec2 deckWorld = { tl + m_car5DeckHb.localCenter.x, MIN_Y + m_car5DeckHb.localCenter.y };
    Math::Vec2 deckSize = m_car5DeckHb.size;
    // 착지·판정 안정화: 발판 위로 약간 확장 (히트박스가 발판과 스킵되는 경우 방지)
    deckSize.y += 72.f;
    deckWorld.y += 36.f;
    return Collision::CheckAABB(hbCenter, hbSize, deckWorld, deckSize);
}


// 플레이어가 Car5 덱 또는 Car5 구역에 진입하면 조우 스크립트를 시작하고 로봇을 재배치함
void Train::TryActivateCar5Encounter(Math::Vec2 hbCenter, Math::Vec2 hbSize)
{
    if (m_car5EncounterActive)
        return;

    bool shouldActivate = IsPlayerOnCar5Deck(hbCenter, hbSize);
    if (!shouldActivate)
    {
        // Deck top 외에도 탱크/밸브 위에 올라탄 경우 활성화되도록 Car5 구역 체크를 허용.
        const float tl = MIN_X + m_trainOffset;
        const float c5 = m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float car5L = tl + c5 - 30.f;
        const float car5R = tl + c5 + m_car5Width + 30.f;
        shouldActivate = (hbCenter.x >= car5L && hbCenter.x <= car5R &&
                          hbCenter.y >= MIN_Y + 120.f && hbCenter.y <= MIN_Y + HEIGHT + 260.f);
    }
    if (!shouldActivate)
        return;

    m_car5EncounterActive = true;
    if (!m_trainCheatCarUnlock)
        m_car5ValveHintTimer = 5.0f;
    m_encounterScriptTime = 0.f;

    // 로봇: 덱 패트롤(1·2·5호차)은 유지 — 레일 재배치는 그 외만
    if (!m_robots.empty())
    {
        const float tl      = MIN_X + m_trainOffset;
        const float c4Right = tl + m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float rail    = Train::MIN_Y + 95.f;
        int railSlot = 0;
        for (size_t i = 0; i < m_robots.size(); ++i)
        {
            Robot& rr = m_robots[i];
            if (rr.IsTrainDeckPatrol())
                continue;
            const float cx = c4Right - 95.f - static_cast<float>(railSlot) * 158.f;
            const float cy = rail + rr.GetSize().y * 0.5f;
            rr.SetPosition({ cx, cy });
            rr.SetGroundLimitY(Train::MIN_Y + 85.f);
            ++railSlot;
        }
    }
    Logger::Instance().Log(Logger::Severity::Event, "Train: FourthTrain deck encounter activated");
}


// 레일 위 로봇이 덱보다 아래 있고 플레이어가 위에 있을 때 점프 보조 속도를 부여함
void Train::AssistRobotRailJumpTowardTrain(Robot& robot, const Player& player, float dt)
{
    (void)dt;
    if (robot.IsDead())
        return;

    const Math::Vec2 pp = player.GetHitboxCenter();
    const Math::Vec2 rp = robot.GetPosition();
    const float      feet = rp.y - robot.GetSize().y * 0.5f;

    if (feet > Train::MIN_Y + 230.f)
        return;
    if (pp.y < rp.y + 90.f)
        return;
    if (robot.GetVelocity().y > 320.f)
        return;

    const RobotState st = robot.GetState();
    if (st != RobotState::Chase && st != RobotState::Patrol && st != RobotState::Windup && st != RobotState::Recover)
        return;

    if (std::abs(robot.GetVelocity().x) > 72.f)
        return;

    const float dx = pp.x - rp.x;
    Math::Vec2  v  = robot.GetVelocity();
    v.x            = std::clamp(dx * 0.19f, -88.f, 88.f);
    v.y            = 695.f;
    robot.SetVelocity(v);
}


// Car5 조우 스크립트 활성 상태에서 열차 로봇 전체의 AI(추격·공격·밸브 도주)를 갱신함
void Train::UpdateTrainRobotsAI(float dt, Player& player)
{
    if (!m_car5EncounterActive || m_robots.empty())
        return;

    const float tl        = MIN_X + m_trainOffset;
    const float trainRight = tl + m_totalTrainWidth;
    const bool  fleeWater = (m_valvePressureT > 0.10f);

    std::vector<ObstacleInfo> obstacleInfos;
    obstacleInfos.reserve(m_trainHitboxes.size());
    for (const auto& hb : m_trainHitboxes)
    {
        if (!hb.collision)
            continue;
        Math::Vec2 world = { tl + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        obstacleInfos.push_back({ world, hb.size });
    }

    const float mapMinX = Train::MIN_X + m_trainOffset - 500.f;
    const float mapMaxX = trainRight + 900.f;

    for (auto& r : m_robots)
    {
        if (r.IsDead())
            continue;
        if (r.IsTrainDeckPatrol())
            continue;

        r.SetGroundLimitY(Train::MIN_Y + 85.f);

        if (fleeWater)
        {
            Math::Vec2 p = r.GetPosition();
            float      nx = p.x + 310.f * dt;
            const float cap = trainRight - 115.f;
            if (nx > cap)
                nx = cap;
            p.x = nx;
            r.SetPosition(p);
            continue;
        }

        const int pcar = GetPlayerTrainCarIndex(player.GetHitboxCenter());
        r.SetAllowTrainCombatVsPlayer(pcar == r.GetTrainCarSegment());

        r.Update(dt, player, obstacleInfos, mapMinX, mapMaxX);
        if (!r.IsTrainDeckPatrol())
            AssistRobotRailJumpTowardTrain(r, player, dt);
    }

    ClampCarSegment4RobotsBeforeValve();
    ClampCar5RobotsEastOfValve();
}



// 전투 드론 벡터의 참조를 반환함
const std::vector<Drone>& Train::GetDrones() const { return m_droneManager->GetDrones(); }
std::vector<Drone>&       Train::GetDrones()        { return m_droneManager->GetDrones(); }

// 사이렌 드론 벡터의 참조를 반환함
const std::vector<Drone>& Train::GetSirenDrones() const { return m_sirenDroneManager->GetDrones(); }

// 전투·사이렌·자동차 운반 드론을 모두 제거함
void Train::ClearAllDrones()
{
    if (m_droneManager)
        m_droneManager->ClearAllDrones();
    if (m_carTransportDroneManager)
        m_carTransportDroneManager->ClearAllDrones();
    if (m_sirenDroneManager)
        m_sirenDroneManager->ClearAllDrones();
}