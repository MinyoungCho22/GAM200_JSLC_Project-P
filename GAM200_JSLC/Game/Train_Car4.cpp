// Train_Car4.cpp - Third_ThirdTrain car transport (6 slots, pulse injection, chain)

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

// ---------------------------------------------------------------------------
// Third_ThirdTrain — 자동차 운반 칸 6슬롯 (엔진 / 펄스 주입 / 직선 연쇄)
// ---------------------------------------------------------------------------
namespace {
constexpr float kCarTransportPulseInjectTotal     = 3.f;
constexpr float kCarTransportPulseInjectDuration = 1.5f;
constexpr float kCarInteractRangeSq   = 600.0f * 600.0f;
constexpr float kCarSkillAnchorRangeSq = 400.0f * 400.0f;
constexpr float kChainDetonateRadius  = 440.0f;
constexpr float kChainDetonateStun    = 2.0f;
constexpr float kMoorePulseShareRate  = 2.5f;

static const int kStraightCarPairs[][2] = {
    { 0, 1 }, { 1, 2 }, { 3, 4 }, { 4, 5 },
    { 0, 3 }, { 1, 4 }, { 2, 5 },
};
}

// 지정 슬롯의 열차 오프셋을 반영한 월드 좌표 중심을 반환함
Math::Vec2 Train::CarTransportWorldCenter(int slotIndex) const
{
    const float trainLeft = MIN_X + m_trainOffset;
    const auto& s         = m_carTransportSlots[static_cast<size_t>(slotIndex)];
    return { trainLeft + s.localCenter.x, MIN_Y + s.localCenter.y };
}

// 두 슬롯이 3×2 그리드에서 무어 인접(8방향 이웃)인지 확인함
bool Train::IsMooreAdjacentCarSlots(int a, int b)
{
    const int ra = a / 3, ca = a % 3;
    const int rb = b / 3, cb = b % 3;
    const int dr = std::abs(ra - rb), dc = std::abs(ca - cb);
    return dr <= 1 && dc <= 1 && (dr + dc) > 0;
}

// 플레이어 히트박스 중심에서 가장 가까운 미시동 슬롯의 인덱스를 반환함 (범위 밖이면 -1)
int Train::FindCarTransportInjectTarget(Math::Vec2 playerHbCenter) const
{
    int   best   = -1;
    float bestD  = 1.e18f;
    for (int i = 0; i < kCarTransportCount; ++i)
    {
        if (m_carTransportSlots[static_cast<size_t>(i)].engineOn)
            continue;
        const Math::Vec2 w = CarTransportWorldCenter(i);
        const float      d = (playerHbCenter - w).LengthSq();
        if (d > kCarInteractRangeSq || d >= bestD)
            continue;
        bestD = d;
        best  = i;
    }
    return best;
}

// 6개 자동차 슬롯을 초기 픽셀 배치·시동 상태(좌상·우하 ON, 나머지 OFF)로 리셋함
void Train::ResetCarTransportSlotsToInitialState()
{
    const float c4 = GetCar4LocalLeft();

    struct Def
    {
        float px, py, pw, ph;
        bool startEngine;
    };
    static const Def defs[kCarTransportCount] = {
        { 429.f, 129.f, 886.f, 304.f, true },   // 좌상 — 시동 ON
        { 1536.f, 129.f, 886.f, 304.f, false },
        { 2629.f, 129.f, 886.f, 304.f, false },
        { 432.f, 489.f, 876.f, 306.f, false },
        { 1539.f, 489.f, 876.f, 306.f, false },
        { 2631.f, 489.f, 876.f, 306.f, true }, // 우하 — 시동 ON
    };

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        Train::TrainHitbox hb = MakeHitbox(c4, defs[i].px, defs[i].py, defs[i].pw, defs[i].ph, false,
                                           TrainHitboxKind::Solid);
        m_carTransportSlots[static_cast<size_t>(i)].localCenter    = hb.localCenter;
        m_carTransportSlots[static_cast<size_t>(i)].halfSize       = hb.size * 0.5f;
        m_carTransportSlots[static_cast<size_t>(i)].engineOn       = defs[i].startEngine;
        m_carTransportSlots[static_cast<size_t>(i)].injectPulseAccum = 0.f;
        m_carTransportSlots[static_cast<size_t>(i)].engineGlowTimer = 0.f;
        m_carTransportSlots[static_cast<size_t>(i)].skipPulseLineOverlay = (i == 0 || i == 5);
    }
    m_carInjectFocusSlot = -1;
}

// 드론 피격 등으로 펄스 주입이 끊겼을 때 모든 슬롯의 누적량을 초기화함
void Train::NotifyCarTransportInjectionInterrupted()
{
    for (auto& s : m_carTransportSlots)
        s.injectPulseAccum = 0.f;
    m_carInjectFocusSlot = -1;
}

// Q 스킬 범위 내에서 가장 가까운 슬롯의 월드 앵커 좌표를 outWorldAnchor에 채워 반환함
bool Train::TryGetCarTransportSkillAnchor(Math::Vec2 playerHbCenter, Math::Vec2& outWorldAnchor) const
{
    int   best  = -1;
    float bestD = 1.e18f;
    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const Math::Vec2 w = CarTransportWorldCenter(i);
        const float      d = (playerHbCenter - w).LengthSq();
        if (d > kCarSkillAnchorRangeSq || d >= bestD)
            continue;
        bestD = d;
        best  = i;
    }
    if (best < 0)
        return false;
    outWorldAnchor = CarTransportWorldCenter(best);
    return true;
}

// Start 아이콘 위에 커서가 놓인 미시동 슬롯의 인덱스를 반환함 (없으면 -1)
int Train::TryGetCarTransportStartInjectSlot(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                             Math::Vec2 mouseWorldPos) const
{
    (void)playerHbSize;
    const Math::Vec2 kCursorHitbox{ 32.f, 32.f };
    constexpr float  kIconW = 110.f;
    float              aspect = 1.f;
    if (m_carTransportStartTex && m_carTransportStartTex->GetWidth() > 0)
        aspect = static_cast<float>(m_carTransportStartTex->GetHeight())
               / static_cast<float>(m_carTransportStartTex->GetWidth());
    const Math::Vec2 iconSize{ kIconW, kIconW * aspect };

    const float trainLeft = MIN_X + m_trainOffset;

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        if (m_carTransportSlots[static_cast<size_t>(i)].engineOn)
            continue;
        const auto&      slot = m_carTransportSlots[static_cast<size_t>(i)];
        const Math::Vec2 wc   = { trainLeft + slot.localCenter.x, MIN_Y + slot.localCenter.y };
        const float      d    = (playerHbCenter - wc).LengthSq();
        if (d > kCarInteractRangeSq)
            continue;

        const Math::Vec2 iconHit{ iconSize.x * 1.2f, iconSize.y * 1.2f };
        if (Collision::CheckPointInAABB(mouseWorldPos, wc, iconHit)
            || Collision::CheckAABB(mouseWorldPos, kCursorHitbox, wc, iconHit))
            return i;
    }
    return -1;
}

// 좌클릭으로 시동 가능한 슬롯을 찾아 outSlotIndex에 채우고 true를 반환함
bool Train::TryGetCarTransportClickIgniteTarget(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                                Math::Vec2 mouseWorldPos, int& outSlotIndex) const
{
    (void)playerHbSize;
    const Math::Vec2 kCursorHitbox{ 32.f, 32.f };

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const auto& slot = m_carTransportSlots[static_cast<size_t>(i)];
        if (slot.engineOn)
            continue;

        const Math::Vec2 wc   = CarTransportWorldCenter(i);
        const Math::Vec2 full = slot.halfSize * 2.f;
        const float      d    = (playerHbCenter - wc).LengthSq();
        if (d > kCarInteractRangeSq)
            continue;

        const bool mouseOnCar = Collision::CheckPointInAABB(mouseWorldPos, wc, full)
                                || Collision::CheckAABB(mouseWorldPos, kCursorHitbox, wc, full);
        if (!mouseOnCar)
            continue;

        outSlotIndex = i;
        return true;
    }
    return false;
}

// 지정 슬롯 시동을 켜고, 직선 연쇄 폭발을 즉시 발동함 (이미 시동 중이면 false 반환)
bool Train::TryIgniteCarTransportSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kCarTransportCount)
        return false;

    auto& slot = m_carTransportSlots[static_cast<size_t>(slotIndex)];
    if (slot.engineOn)
        return false;

    slot.engineOn        = true;
    slot.injectPulseAccum = 0.f;
    slot.engineGlowTimer = 1.35f;
    if (m_carInjectFocusSlot == slotIndex)
        m_carInjectFocusSlot = -1;

    FireStraightLineChainDetonations();

    Logger::Instance().Log(Logger::Severity::Info,
                           "Train: Car transport engine ON (click slot %d)", slotIndex);
    return true;
}

// 시동된 슬롯 쌍 사이 직선 아크 좌표를 outArcs에 추가함 (두 슬롯 모두 시동 상태일 때만)
void Train::AppendCarTransportStraightChainArcs(std::vector<std::pair<Math::Vec2, Math::Vec2>>& outArcs) const
{
    constexpr int pairCount = static_cast<int>(sizeof(kStraightCarPairs) / sizeof(kStraightCarPairs[0]));
    for (int e = 0; e < pairCount; ++e)
    {
        const int a = kStraightCarPairs[e][0];
        const int b = kStraightCarPairs[e][1];

        if (!m_carTransportSlots[static_cast<size_t>(a)].engineOn
            || !m_carTransportSlots[static_cast<size_t>(b)].engineOn)
            continue;

        outArcs.push_back({ CarTransportWorldCenter(a), CarTransportWorldCenter(b) });
    }
}

// Q 펄스 반경 내 호버 드론을 경유해 시동된 차량으로 연결되는 가지 아크를 outArcs에 추가함
void Train::AppendCarTransportPulseBranchArcs(std::vector<std::pair<Math::Vec2, Math::Vec2>>& outArcs,
                                              Math::Vec2 pulseWorldCenter, float pulseRadius) const
{
    if (!m_carTransportDroneManager)
        return;

    const float    pulseR2     = pulseRadius * pulseRadius;
    constexpr float kCarDroneMax   = 580.f;
    const float     kCarDroneMaxSq = kCarDroneMax * kCarDroneMax;
    constexpr float kDroneBranchMax   = 420.f;
    const float     kDroneBranchMaxSq = kDroneBranchMax * kDroneBranchMax;

    int          litSlot[kCarTransportCount];
    Math::Vec2   litPos[kCarTransportCount];
    int litCount = 0;
    for (int s = 0; s < kCarTransportCount; ++s)
    {
        if (!m_carTransportSlots[static_cast<size_t>(s)].engineOn)
            continue;
        litSlot[litCount] = s;
        litPos[litCount] = CarTransportWorldCenter(s);
        ++litCount;
    }
    if (litCount == 0)
        return;

    const auto& drones = m_carTransportDroneManager->GetDrones();

    struct NearD
    {
        size_t     idx{};
        int        carSlot{};
        Math::Vec2 carCenter{};
        float      distCarSq{};
        Math::Vec2 dronePos{};
    };
    std::vector<NearD> near;
    near.reserve(drones.size());

    for (size_t i = 0; i < drones.size(); ++i)
    {
        const Drone& d = drones[i];
        if (d.IsDead())
            continue;
        const Math::Vec2 dp = d.GetPosition();
        if ((dp - pulseWorldCenter).LengthSq() > pulseR2)
            continue;

        int   bestLitI = 0;
        float bestD    = (dp - litPos[0]).LengthSq();
        for (int L = 1; L < litCount; ++L)
        {
            const float dsq = (dp - litPos[L]).LengthSq();
            if (dsq < bestD)
            {
                bestD    = dsq;
                bestLitI = L;
            }
        }
        if (bestD > kCarDroneMaxSq)
            continue;
        near.push_back({ i, litSlot[bestLitI], litPos[bestLitI], bestD, dp });
    }
    if (near.empty())
        return;

    std::sort(near.begin(), near.end(), [](const NearD& A, const NearD& B) {
        if (A.carSlot != B.carSlot)
            return A.carSlot < B.carSlot;
        return A.distCarSq < B.distCarSq;
    });

    for (size_t i = 0; i < near.size(); ++i)
    {
        const NearD& nd = near[i];
        outArcs.push_back({ nd.carCenter, nd.dronePos });

        if (i + 1 < near.size())
        {
            const NearD& n2 = near[i + 1];
            if (n2.carSlot == nd.carSlot)
            {
                const float bridgesq = (n2.dronePos - nd.dronePos).LengthSq();
                if (bridgesq <= kDroneBranchMaxSq && bridgesq > 4.f)
                    outArcs.push_back({ nd.dronePos, n2.dronePos });
            }
        }
    }
}

// 무어 연결된 시동 슬롯 클러스터에 매 프레임 펄스를 공유 지급함
void Train::ApplyMooreConnectedPulseShare(Player& player, float dt)
{
    bool visited[kCarTransportCount] = {};
    float bonus                       = 0.f;
    int  q[kCarTransportCount];

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        if (!m_carTransportSlots[static_cast<size_t>(i)].engineOn || visited[i])
            continue;

        std::size_t head = 0;
        std::size_t tail = 0;
        q[tail++] = i;
        visited[i] = true;
        int cluster = 0;

        while (head < tail)
        {
            const int u = q[head++];
            ++cluster;
            for (int v = 0; v < kCarTransportCount; ++v)
            {
                if (!m_carTransportSlots[static_cast<size_t>(v)].engineOn || visited[v])
                    continue;
                if (!IsMooreAdjacentCarSlots(u, v))
                    continue;
                visited[v] = true;
                q[tail++] = v;
            }
        }

        if (cluster >= 2)
            bonus += static_cast<float>(cluster - 1) * kMoorePulseShareRate * dt;
    }

    if (bonus > 0.f)
        player.GetPulseCore().getPulse().add(bonus);
}

// 직선 쌍(kStraightCarPairs)으로 연결된 두 슬롯이 모두 시동 상태일 때 범위 내 드론에게 연쇄 폭발을 적용함
void Train::FireStraightLineChainDetonations()
{
    if (!m_droneManager && !m_carTransportDroneManager)
        return;

    constexpr int pairCount = static_cast<int>(sizeof(kStraightCarPairs) / sizeof(kStraightCarPairs[0]));
    for (int e = 0; e < pairCount; ++e)
    {
        const int        a = kStraightCarPairs[e][0];
        const int        b = kStraightCarPairs[e][1];
        if (!m_carTransportSlots[static_cast<size_t>(a)].engineOn
            || !m_carTransportSlots[static_cast<size_t>(b)].engineOn)
            continue;

        const Math::Vec2 wa  = CarTransportWorldCenter(a);
        const Math::Vec2 wb  = CarTransportWorldCenter(b);
        const Math::Vec2 mid = { (wa.x + wb.x) * 0.5f, (wa.y + wb.y) * 0.5f };
        if (m_droneManager)
            m_droneManager->ApplyDetonation(mid, kChainDetonateRadius, kChainDetonateStun);
        if (m_carTransportDroneManager)
            m_carTransportDroneManager->ApplyDetonation(mid, kChainDetonateRadius, kChainDetonateStun);
    }
}

// 자동차 운반 칸 전체 로직(펄스 주입·연쇄·무어 공유·드론)을 매 프레임 갱신함
void Train::UpdateCarTransport(float dt, Player& player, Math::Vec2 playerHbCenter,
                               bool injectHeld, bool ignorePulseCost, int forcedInjectSlot)
{
    const float fdt = dt;

    for (auto& s : m_carTransportSlots)
    {
        if (s.engineGlowTimer > 0.f)
            s.engineGlowTimer = std::max(0.f, s.engineGlowTimer - fdt);
    }

    ApplyMooreConnectedPulseShare(player, fdt);

    int focus = -1;
    if (forcedInjectSlot >= 0 && forcedInjectSlot < kCarTransportCount)
    {
        auto& fs = m_carTransportSlots[static_cast<size_t>(forcedInjectSlot)];
        if (!fs.engineOn)
        {
            const Math::Vec2 w = CarTransportWorldCenter(forcedInjectSlot);
            const float      d = (playerHbCenter - w).LengthSq();
            if (d <= kCarInteractRangeSq)
                focus = forcedInjectSlot;
        }
    }
    if (focus < 0 && injectHeld)
        focus = FindCarTransportInjectTarget(playerHbCenter);

    if (!injectHeld || focus < 0)
    {
        if (m_carInjectFocusSlot >= 0)
        {
            m_carTransportSlots[static_cast<size_t>(m_carInjectFocusSlot)].injectPulseAccum = 0.f;
            m_carInjectFocusSlot                                                             = -1;
        }
        return;
    }

    if (m_carInjectFocusSlot >= 0 && m_carInjectFocusSlot != focus)
        m_carTransportSlots[static_cast<size_t>(m_carInjectFocusSlot)].injectPulseAccum = 0.f;

    m_carInjectFocusSlot = focus;
    auto& slot           = m_carTransportSlots[static_cast<size_t>(focus)];

    if (slot.engineOn)
    {
        slot.injectPulseAccum = 0.f;
        m_carInjectFocusSlot = -1;
        return;
    }

    Pulse&        pulse       = player.GetPulseCore().getPulse();
    const float   pulsePerSec = kCarTransportPulseInjectTotal / kCarTransportPulseInjectDuration;
    const float   needPulse   = pulsePerSec * fdt;

    if (!ignorePulseCost)
    {
        if (pulse.Value() < needPulse)
        {
            slot.injectPulseAccum = 0.f;
            m_carInjectFocusSlot = -1;
            return;
        }
        pulse.spend(needPulse);
    }

    slot.injectPulseAccum += needPulse;
    if (slot.injectPulseAccum >= kCarTransportPulseInjectTotal)
    {
        slot.engineOn         = true;
        slot.injectPulseAccum = 0.f;
        slot.engineGlowTimer  = 1.35f;
        m_carInjectFocusSlot  = -1;
        FireStraightLineChainDetonations();
        Logger::Instance().Log(Logger::Severity::Info,
                               "Train: Car transport engine ON (slot %d)", focus);
    }
}

// Car4 호버 드론의 앵커 위치 갱신·물 피해·추적 로직을 매 프레임 처리함
void Train::UpdateCarTransportDrones(float dt, const Player& player, Math::Vec2 playerHitboxSize,
                                     bool isPlayerHidingTrain)
{
    if (!m_carTransportDroneManager)
        return;

    const float        c4        = GetCar4LocalLeft();
    const float        trainLeft = MIN_X + m_trainOffset;
    auto&              drones = m_carTransportDroneManager->GetDrones();
    const Math::Vec2   pHb    = player.GetHitboxCenter();
    const bool         inPulseBox = IsPlayerInCar2PurplePulseBox(pHb, playerHitboxSize);
    const bool         undetect   = isPlayerHidingTrain || inPulseBox;

    for (size_t i = 0; i < drones.size(); ++i)
    {
        Drone& d = drones[i];
        if (static_cast<int>(i) < kCarTransportHoverDroneCount && d.IsCarTransportPersistHover())
        {
            const float lx = c4 + kCarTransportDronePixels[i].x;
            const float ly = ASSUMED_IMG_HEIGHT - kCarTransportDronePixels[i].yTop;
            d.SetCarTransportAnchorWorld({ trainLeft + lx, MIN_Y + ly });
            d.SetCarTransportJamConfused(inPulseBox || isPlayerHidingTrain);
        }
        d.Update(dt, player, playerHitboxSize, undetect, true, 1.f);
        if (static_cast<int>(i) < kCarTransportHoverDroneCount && d.IsCarTransportPersistHover() && !d.IsDead()
            && !d.IsHit() && !d.IsCarTransportAggroChase())
            d.SetCarTransportHover(true);
    }
}


// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------

// 현재 주입 중인 슬롯의 펄스 주입 진행도를 슬롯 위에 세로 게이지로 그림
void Train::DrawCarTransportInjectProgressGauge(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_skyVAO)
        return;
    if (m_carInjectFocusSlot < 0 || m_carInjectFocusSlot >= kCarTransportCount)
        return;

    const auto& slot = m_carTransportSlots[static_cast<size_t>(m_carInjectFocusSlot)];
    if (slot.engineOn)
        return;

    const float trainLeft = MIN_X + m_trainOffset;
    const Math::Vec2 posC = { trainLeft + slot.localCenter.x, MIN_Y + slot.localCenter.y };
    const Math::Vec2 half = slot.halfSize;

    const float barW            = 10.0f;
    const float barH            = std::min(110.0f, std::max(44.0f, half.y * 2.f * 0.9f));
    const float padY            = 10.0f;
    const float horizNudgeRight = 38.0f;
    const float vertNudgeDown   = -8.0f;

    const float rx = posC.x + half.x;
    const float ty = posC.y + half.y;
    const float barCenterX = rx - 10.0f - barW * 0.5f + horizNudgeRight;
    const float innerTopY    = ty + padY + vertNudgeDown;
    const float innerCenterY = innerTopY - barH * 0.5f;

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (barCenterX < visLeft || barCenterX > visRight)
        return;

    colorShader.use();

    const Math::Vec2 bgSize = { barW + 6.0f, barH + 6.0f };
    DrawFilledQuad(colorShader, { barCenterX, innerCenterY }, bgSize, 0.2f, 0.2f, 0.2f, 1.0f);

    DrawFilledQuad(colorShader, { barCenterX, innerCenterY }, { barW, barH }, 0.14f, 0.14f, 0.18f, 1.0f);

    const float t     = std::clamp(slot.injectPulseAccum / kCarTransportPulseInjectTotal, 0.f, 1.f);
    const float fillH = barH * t;
    if (fillH > 0.5f)
    {
        const float innerBottomY = innerTopY - barH;
        const float fillCenterY  = innerBottomY + fillH * 0.5f;
        DrawFilledQuad(colorShader, { barCenterX, fillCenterY }, { barW, fillH }, 0.25f, 0.85f, 1.0f, 1.0f);
    }
}



// 시동 ON/OFF 슬롯에 PulseLine·Start 아이콘 오버레이를 카메라 가시 범위 내에서 그림
void Train::DrawCarTransportOverlays(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    float halfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float margin   = 900.0f;
    const float visLeft  = cameraPos.x - halfW - margin;
    const float visRight = cameraPos.x + halfW + margin;
    const float trainLeft = MIN_X + m_trainOffset;

    textureShader.use();
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);
    textureShader.setFloat("alpha", 1.0f);

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const auto& slot = m_carTransportSlots[static_cast<size_t>(i)];
        const Math::Vec2 wc = { trainLeft + slot.localCenter.x, MIN_Y + slot.localCenter.y };
        if (wc.x + slot.halfSize.x < visLeft || wc.x - slot.halfSize.x > visRight)
            continue;

        if (slot.engineOn && !slot.skipPulseLineOverlay)
        {
            const bool useLeft = (i == 2);
            Background* tex = useLeft ? m_carTransportPulseLeftTex.get() : m_carTransportPulseRightTex.get();
            if (!tex || tex->GetWidth() <= 0)
                continue;

            // ─── PulseLine 오버레이 월드 좌표 (시각 조정용) ─────────────────────
            // 기준점 wc = CarTransportWorldCenter(i) 와 동일 — 슬롯 히트박스 중심.
            // 펄스 스프라이트 **중심** 최종 위치: ( wc.x + 오프셋X, wc.y )
            // 요청 오프셋: Left 텍스처 -30, Right 텍스처 -24 (차 중심 대비 X만).
            const float ox = useLeft ? -30.f : -24.f;
            const Math::Vec2 pulseCenter{ wc.x + ox, wc.y };

            const float      carW      = slot.halfSize.x * 2.f;
            const float      scaleW    = carW * 0.98f;
            const float      texAspect = static_cast<float>(tex->GetHeight()) / static_cast<float>(tex->GetWidth());
            const Math::Vec2 scale{ scaleW, scaleW * texAspect };

            const Math::Matrix model =
                Math::Matrix::CreateTranslation(pulseCenter) * Math::Matrix::CreateScale(scale);
            tex->Draw(textureShader, model);
        }
        else if (!slot.engineOn && m_carTransportStartTex && m_carTransportStartTex->GetWidth() > 0)
        {
            constexpr float iconW = 110.f;
            const float     aspect = static_cast<float>(m_carTransportStartTex->GetHeight())
                                 / static_cast<float>(m_carTransportStartTex->GetWidth());
            const Math::Matrix model =
                Math::Matrix::CreateTranslation(wc) * Math::Matrix::CreateScale({ iconW, iconW * aspect });
            m_carTransportStartTex->Draw(textureShader, model);
        }
    }
}


// ---------------------------------------------------------------------------
// DrawCarTransportVFX — 시동된 차량 상단 펄스 라이트 이펙트를 그림
// ---------------------------------------------------------------------------
void Train::DrawCarTransportVFX(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_skyVAO)
        return;

    float halfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float margin  = 900.0f;
    const float visLeft = cameraPos.x - halfW - margin;
    const float visRight = cameraPos.x + halfW + margin;

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float trainLeft = MIN_X + m_trainOffset;

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const auto& s = m_carTransportSlots[static_cast<size_t>(i)];
        if (!s.engineOn && s.engineGlowTimer <= 0.f)
            continue;

        const Math::Vec2 wc = { trainLeft + s.localCenter.x, MIN_Y + s.localCenter.y };
        if (wc.x + s.halfSize.x < visLeft || wc.x - s.halfSize.x > visRight)
            continue;

        const float flash = (s.engineGlowTimer > 0.f) ? std::min(1.f, s.engineGlowTimer * 2.f) : 0.f;
        const float baseA = s.engineOn ? 0.20f : 0.f;
        const float a     = std::min(1.f, baseA + flash * 0.55f);

        DrawFilledQuad(colorShader, wc, { s.halfSize.x * 1.65f, s.halfSize.y * 0.52f }, 0.22f, 0.82f, 1.0f, a);
        if (flash > 0.02f)
            DrawFilledQuad(colorShader, wc, { s.halfSize.x * 2.15f, s.halfSize.y * 0.82f }, 0.35f, 0.92f, 1.0f,
                           flash * 0.38f);
    }
}
