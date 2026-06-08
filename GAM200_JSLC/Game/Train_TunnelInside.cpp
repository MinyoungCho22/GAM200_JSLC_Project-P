// Train_TunnelInside.cpp - Turnel_Inside props, Object.png, Q-skill knockback

#include "Train_Internal.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kTunnelInjectPulseTotal   = 5.f;
constexpr float kTunnelInjectDurationSec  = 1.5f;
// SecondTrain_3 우측에서 보이는 폭(조금 더 왼쪽까지)
constexpr float kTunnelCar3VisibleFrac = 0.38f;

float TunnelInsideCar3Width(const Train& train)
{
    return train.GetTunnelInsideCar3WorldWidth();
}

// SecondTrain_3 가시 슬라이스(원본 비율)의 월드 왼쪽 X
float TunnelInsideCar3SliceLeft(const Train& train)
{
    return train.GetTunnelInsideWorldLeft() + train.GetTunnelInsideTrainVisualX();
}

}

bool Train::ShouldHideTrainExteriorHazards() const
{
    // SecondInside(SecondTrain_1 문) 진입·터널 전환·터널 인사이드 동안 외부(사이렌) 연출 차단
    if (m_car3InsideViewActive)
        return true;
    if (m_car3InsideTransitionActive && m_car3InsideTransitionTargetInside)
        return true;
    if (m_car3TunnelInsideViewActive)
        return true;
    if (m_car3TunnelInsideTransitionActive && m_car3TunnelInsideTransitionTargetInside)
        return true;
    return false;
}

float Train::GetTunnelInsideDeckSurfaceY() const
{
    return MIN_Y + kTunnelInsideTrainDeckTopLocalY;
}

float Train::GetTunnelInsideExteriorDeckSurfaceY() const
{
    return MIN_Y + kTrainFlatbedDeckTopLocalY;
}

void Train::GetTunnelInsideBoardingFloor(Math::Vec2& outCenter, Math::Vec2& outSize) const
{
    const float car3W  = TunnelInsideCar3Width(*this);
    const float sliceW = car3W * kTunnelCar3VisibleFrac;
    float       frontW = 0.f;
    if (m_secondTrainFrontTex && m_secondTrainFrontTex->GetWidth() > 0)
        frontW = static_cast<float>(m_secondTrainFrontTex->GetWidth());

    const float sliceLeft  = TunnelInsideCar3SliceLeft(*this);
    const float trainRight = sliceLeft + sliceW + frontW;
    const float leftEdge   = sliceLeft - 800.f; // Extended significantly to the left
    const float platW      = std::max(120.f, trainRight - leftEdge + 32.f);
    const float deckCy = MIN_Y + kTunnelInsideTrainDeckCenterLocalY;
    outSize            = { platW, kTunnelInsideTrainDeckImagePh };
    outCenter          = { leftEdge + platW * 0.5f, deckCy };
}

// 출발 후: 터널 왼쪽 끝부터 이동 중인 열차 앞까지 이어지는 발판(걸어서 탑승 가능)
void Train::GetTunnelInsideDepartWalkFloor(Math::Vec2& outCenter, Math::Vec2& outSize) const
{
    const float car3W  = TunnelInsideCar3Width(*this);
    const float sliceW = car3W * kTunnelCar3VisibleFrac;
    float       frontW = 0.f;
    if (m_secondTrainFrontTex && m_secondTrainFrontTex->GetWidth() > 0)
        frontW = static_cast<float>(m_secondTrainFrontTex->GetWidth());

    const float trainLeft  = TunnelInsideCar3SliceLeft(*this);
    const float trainRight = trainLeft + sliceW + frontW;
    const float leftEdge   = m_tunnelInsideWorldLeft;
    const float rightEdge  = std::min(trainRight, m_tunnelInsideWorldLeft + m_tunnelInsideWorldWidth - 24.f);
    const float width      = std::max(120.f, rightEdge - leftEdge);
    const float railTop    = GetRailWalkSurfaceWorldY();
    constexpr float kSlabH = 36.f;
    outSize                = { width, kSlabH };
    outCenter              = { leftEdge + width * 0.5f, railTop - kSlabH * 0.5f };
}

float Train::GetTunnelInsideCar3WorldWidth() const
{
    return m_car3ExtensionWidths[2];
}

Math::Vec2 Train::GetTunnelInsideInjectPropWorldCenter() const
{
    if (m_tunnelInsideProps.size() < 2 || !m_tunnelInsideProps[1].injectable)
        return {};
    const auto& prop = m_tunnelInsideProps[1];
    return { m_tunnelInsideWorldLeft + prop.localCenter.x, MIN_Y + prop.localCenter.y };
}

void Train::InitTunnelInsideProps()
{
    m_tunnelInsideProps.clear();
    m_tunnelInsideInjectT        = 0.f;
    m_tunnelInsideInjectComplete = false;
    m_tunnelInsideDepartStarted  = false;
    m_tunnelInsideTrainVisualX         = 0.f;
    m_tunnelInsideDepartTrainOffset0   = 0.f;
    m_tunnelInsideBoardTimer           = -1.f;
    m_playerOnTunnelBoardingFloor        = false;
    m_playerOnTunnelDepartWalkFloor      = false;

    // Turnel_Inside.png 좌표 — 왼쪽 노란 오브젝트 → Object.png
    {
        const TrainHitbox hb = MakeHitbox(0.f, 590.f, 753.f, 178.f, 256.f, false);
        TunnelInsideProp    p{};
        p.localCenter      = hb.localCenter;
        p.size             = hb.size;
        p.useObjectSprite  = true;
        p.pushable         = true;
        m_tunnelInsideProps.push_back(p);
    }
    // 우측 오브젝트 — 좌클릭 펄스 주입
    {
        const TrainHitbox hb = MakeHitbox(0.f, 2250.f, 891.f, 126.f, 120.f, false);
        TunnelInsideProp    p{};
        p.localCenter = hb.localCenter;
        p.size        = hb.size;
        p.injectable  = true;
        m_tunnelInsideProps.push_back(p);
    }
}

void Train::UpdateTunnelInsideInject(float dt, Player& player, Math::Vec2 playerHbCenter,
                                     Math::Vec2 playerHitboxSize, Math::Vec2 mouseWorldPos, bool attackHeld,
                                     bool injectGodMode)
{
    if (!m_car3TunnelInsideViewActive || m_tunnelInsideInjectComplete || m_tunnelInsideProps.size() < 2)
        return;

    const auto&      prop   = m_tunnelInsideProps[1];
    if (!prop.injectable)
        return;

    const Math::Vec2 worldC = GetTunnelInsideInjectPropWorldCenter();
    const Math::Vec2 range  = { prop.size.x + 200.f, prop.size.y + 180.f };
    const bool       inRange =
        Collision::CheckAABB(playerHbCenter, playerHitboxSize, worldC, range);
    const bool cursorOnObj = Collision::CheckPointInAABB(mouseWorldPos, worldC, prop.size)
                             || Collision::CheckAABB(mouseWorldPos, { 24.f, 24.f }, worldC, prop.size);

    // Debug: log inject state every ~0.5 s when attack is held
    {
        static float sDbgTimer = 0.f;
        sDbgTimer += dt;
        if (attackHeld && sDbgTimer >= 0.5f)
        {
            sDbgTimer = 0.f;
            Logger::Instance().Log(Logger::Severity::Info,
                "TunnelInject DBG: attackHeld=%d inRange=%d cursorOnObj=%d pulse=%.1f "
                "propW=(%.0f,%.0f) propSz=(%.0f,%.0f) mouse=(%.0f,%.0f) playerHb=(%.0f,%.0f)",
                (int)attackHeld, (int)inRange, (int)cursorOnObj,
                player.GetPulseCore().getPulse().Value(),
                worldC.x, worldC.y, prop.size.x, prop.size.y,
                mouseWorldPos.x, mouseWorldPos.y,
                playerHbCenter.x, playerHbCenter.y);
        }
    }

    if (attackHeld && inRange && cursorOnObj)
    {
        const float pulsePerSec    = kTunnelInjectPulseTotal / kTunnelInjectDurationSec;
        float       pulseThisFrame = pulsePerSec * dt;
        if (!injectGodMode)
        {
            pulseThisFrame = std::min(pulseThisFrame, player.GetPulseCore().getPulse().Value());
            if (pulseThisFrame > 0.f)
                player.GetPulseCore().getPulse().spend(pulseThisFrame);
        }
        if (pulseThisFrame > 0.f || injectGodMode)
        {
            const float deltaT =
                injectGodMode ? (dt / kTunnelInjectDurationSec) : (pulseThisFrame / kTunnelInjectPulseTotal);
            m_tunnelInsideInjectT += deltaT;
        }
        if (m_tunnelInsideInjectT >= 1.f)
        {
            m_tunnelInsideInjectT        = 1.f;
            m_tunnelInsideInjectComplete = true;
            Logger::Instance().Log(Logger::Severity::Info,
                "Train: Turnel_Inside inject complete — train will depart.");
        }
    }
}

bool Train::IsTunnelInsideInjectHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                        Math::Vec2 mouseWorld) const
{
    if (!m_car3TunnelInsideViewActive || m_tunnelInsideInjectComplete || m_tunnelInsideProps.size() < 2)
        return false;
    const auto& prop = m_tunnelInsideProps[1];
    if (!prop.injectable)
        return false;

    const Math::Vec2 worldC = GetTunnelInsideInjectPropWorldCenter();
    const Math::Vec2 range  = { prop.size.x + 200.f, prop.size.y + 180.f };
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, worldC, range))
        return false;
    return Collision::CheckPointInAABB(mouseWorld, worldC, prop.size)
           || Collision::CheckAABB(mouseWorld, { 24.f, 24.f }, worldC, prop.size);
}

bool Train::IsPlayerOnTunnelInsideBoardingSlice(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                                                bool /*onGround*/) const
{
    if (!m_car3TunnelInsideViewActive || !m_tunnelInsideInjectComplete || !m_tunnelInsideDepartStarted)
        return false;

    Math::Vec2 floorC{};
    Math::Vec2 floorS{};
    GetTunnelInsideBoardingFloor(floorC, floorS);

    const float pMinX = playerHbCenter.x - playerHitboxSize.x * 0.5f;
    const float pMaxX = playerHbCenter.x + playerHitboxSize.x * 0.5f;
    const float fMinX = floorC.x - floorS.x * 0.5f;
    const float fMaxX = floorC.x + floorS.x * 0.5f;

    // Check horizontal overlap
    if (pMaxX <= fMinX || pMinX >= fMaxX)
        return false;

    const float platTop = GetTunnelInsideDeckSurfaceY();
    const float feet    = playerHbCenter.y - playerHitboxSize.y * 0.5f;
    // Keep it robust against physics jitter or frame ticks
    const bool ok = (feet <= platTop + 24.f && feet >= platTop - 24.f);
    static float sDbgT = 0.f;
    // rough tick timer using approximate delta
    sDbgT += 0.016f; 
    if (sDbgT >= 0.5f)
    {
        sDbgT = 0.f;
        Logger::Instance().Log(Logger::Severity::Info,
            "BoardingSlice DBG: feet=%.1f platTop=%.1f (diff=%.1f) ok=%d playerX=%.1f floorL=%.1f floorR=%.1f",
            feet, platTop, feet - platTop, (int)ok,
            playerHbCenter.x, fMinX, fMaxX);
    }
    return ok;
}

void Train::SnapPlayerToSecondTrain3Return(Player& player, Math::Vec2 playerHitboxSize)
{
    m_car3InsideOnRoof = false;
    const float      tl       = MIN_X + m_trainOffset;
    const float      car3Left = tl + m_car1Width + m_car2Width + m_car3Width + m_car3ExtensionWidths[0]
                           + m_car3ExtensionWidths[1];
    const float      car3W    = m_car3ExtensionWidths[2];
    const float      floorTop = GetTunnelInsideExteriorDeckSurfaceY();
    const float      halfH    = playerHitboxSize.y * 0.5f;
    const Math::Vec2 oldHb    = player.GetHitboxCenter();
    const Math::Vec2 newHb    = { car3Left + car3W * 0.88f, floorTop + halfH };
    player.SetPosition(player.GetPosition() + (newHb - oldHb));
    player.SetCurrentGroundLevel(floorTop);
    player.ResetVelocity();
    player.SetOnGround(true);
    m_tunnelInsideCameraSnapPending = true;
}

void Train::DrawTunnelInsideInjectGauge(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car3TunnelInsideViewActive || m_tunnelInsideInjectComplete || m_tunnelInsideProps.size() < 2
        || !m_tunnelInsideProps[1].injectable || !m_skyVAO)
        return;

    const auto&      prop   = m_tunnelInsideProps[1];
    const Math::Vec2 posC   = GetTunnelInsideInjectPropWorldCenter();
    const Math::Vec2 half   = prop.size * 0.5f;

    const float barW            = 10.0f;
    const float barH            = std::min(110.0f, std::max(44.0f, prop.size.y * 0.95f));
    const float padY            = 10.0f;
    const float horizNudgeRight = 38.0f;
    const float vertNudgeDown   = -8.0f;

    const float rx           = posC.x + half.x;
    const float ty           = posC.y + half.y;
    const float barCenterX   = rx - 10.0f - barW * 0.5f + horizNudgeRight;
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

    const float t     = std::clamp(m_tunnelInsideInjectT, 0.f, 1.f);
    const float fillH = barH * t;
    if (fillH > 0.5f)
    {
        const float innerBottomY = innerTopY - barH;
        const float fillCenterY  = innerBottomY + fillH * 0.5f;
        DrawFilledQuad(colorShader, { barCenterX, fillCenterY }, { barW, fillH }, 0.25f, 0.75f, 1.0f, 1.0f);
    }
}

void Train::ApplyPulseToTunnelInsideProps(Math::Vec2 pulseWorldCenter, float radius)
{
    if (!m_car3TunnelInsideViewActive || m_tunnelInsideProps.empty())
        return;

    const Math::Vec2 pulseHalf = { radius, radius };
    constexpr float kKnockSpeedX = 1800.f;  // 더 멀리 밀림

    for (auto& prop : m_tunnelInsideProps)
    {
        if (!prop.pushable)
            continue;

        const Math::Vec2 worldC = { m_tunnelInsideWorldLeft + prop.localCenter.x,
                                    MIN_Y + prop.localCenter.y };
        const Math::Vec2 detect = { prop.size.x + 48.f, prop.size.y + 48.f };
        if (!Collision::CheckAABB(pulseWorldCenter, pulseHalf, worldC, detect))
            continue;

        prop.velocity.x      = std::max(prop.velocity.x, kKnockSpeedX);
        prop.pushFlashTimer  = 0.35f;  // 0.35초 밝은 flash 이펙트
        Logger::Instance().Log(Logger::Severity::Info, "Train: Q pulse knocked tunnel Object to the right.");
    }
}

void Train::UpdateTunnelInsideProps(float dt, Player& /*player*/, Math::Vec2 /*playerHitboxSize*/)
{
    if (!m_car3TunnelInsideViewActive || m_tunnelInsideProps.empty())
        return;

    const float friction = std::pow(0.08f, dt);

    for (auto& prop : m_tunnelInsideProps)
    {
        // 플래시 타이머 탄클
        if (prop.pushFlashTimer > 0.f)
            prop.pushFlashTimer -= dt;

        if (!prop.pushable || std::abs(prop.velocity.x) <= 1.f)
            continue;

        prop.localCenter.x += prop.velocity.x * dt;
        prop.velocity.x *= friction;

        const float halfW = prop.size.x * 0.5f;
        const float minX  = halfW + 24.f;
        const float maxX  = std::max(minX, m_tunnelInsideWorldWidth - halfW - 24.f);
        prop.localCenter.x = std::clamp(prop.localCenter.x, minX, maxX);
    }
}

void Train::DrawTunnelInsideBackground(Shader& shader) const
{
    if (!m_car3TunnelInsideViewActive)
        return;

    shader.setFloat("alpha", 1.0f);
    shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    shader.setFloat("tintStrength", 0.0f);

    if (m_tunnelInsideTrain && m_tunnelInsideTrain->GetWidth() > 0)
    {
        shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
        const float w  = m_tunnelInsideWorldWidth;
        const float cx = m_tunnelInsideWorldLeft + w * 0.5f;
        const float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) * Math::Matrix::CreateScale({ w, HEIGHT });
        m_tunnelInsideTrain->Draw(shader, model);
    }
}

void Train::DrawTunnelInsideTrainForeground(Shader& shader) const
{
    if (!m_car3TunnelInsideViewActive)
        return;

    shader.setFloat("alpha", 1.0f);
    shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    shader.setFloat("tintStrength", 0.0f);

    Background* car3Tex = m_car3ExtensionTrains[2].get();
    const float   car3W = TunnelInsideCar3Width(*this);
    const float   cy    = MIN_Y + HEIGHT * 0.5f;
    if (car3Tex && car3Tex->GetWidth() > 0 && car3W > 0.f)
    {
        const float sliceLeft = TunnelInsideCar3SliceLeft(*this);
        const float cx        = sliceLeft - car3W * (0.5f - kTunnelCar3VisibleFrac);
        shader.setVec4("spriteRect", 1.f - kTunnelCar3VisibleFrac, 0.f, kTunnelCar3VisibleFrac, 1.f);
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) * Math::Matrix::CreateScale({ car3W, HEIGHT });
        car3Tex->Draw(shader, model);
        shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);

        if (m_secondTrainFrontTex && m_secondTrainFrontTex->GetWidth() > 0)
        {
            const float frontW       = static_cast<float>(m_secondTrainFrontTex->GetWidth());
            const float quarterRight = sliceLeft + car3W * kTunnelCar3VisibleFrac;
            const float frontCx      = quarterRight + frontW * 0.5f;
            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            Math::Matrix frontModel =
                Math::Matrix::CreateTranslation({ frontCx, cy }) * Math::Matrix::CreateScale({ frontW, HEIGHT });
            m_secondTrainFrontTex->Draw(shader, frontModel);
        }
    }
}

void Train::DrawTunnelInsideComposite(Shader& shader) const
{
    DrawTunnelInsideBackground(shader);
    DrawTunnelInsideTrainForeground(shader);
}

void Train::DrawTunnelInsideProps(Shader& shader) const
{
    if (!m_car3TunnelInsideViewActive)
        return;

    for (const auto& prop : m_tunnelInsideProps)
    {
        if (prop.injectable)
        {
            if (m_tunnelPulseInjectorSprite && m_tunnelPulseInjectorSprite->GetWidth() > 0)
            {
                const Math::Vec2 worldC = { m_tunnelInsideWorldLeft + prop.localCenter.x,
                                            MIN_Y + prop.localCenter.y };
                Math::Matrix     model  = Math::Matrix::CreateTranslation(worldC)
                                       * Math::Matrix::CreateScale(prop.size);
                shader.setFloat("alpha", 1.0f);
                shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
                shader.setFloat("tintStrength", 0.0f);
                m_tunnelPulseInjectorSprite->Draw(shader, model);
            }
            continue;
        }

        if (!prop.useObjectSprite || !m_tunnelObjectTex || m_tunnelObjectTex->GetWidth() <= 0)
            continue;

        const Math::Vec2 worldC = { m_tunnelInsideWorldLeft + prop.localCenter.x,
                                    MIN_Y + prop.localCenter.y };
        Math::Matrix     model  = Math::Matrix::CreateTranslation(worldC)
                               * Math::Matrix::CreateScale(prop.size);

        if (prop.pushFlashTimer > 0.f)
        {
            // 충격 후 0.35초동안 파란색 밝은 flash 효과
            const float t = prop.pushFlashTimer / 0.35f;  // 1닥 밝게 시작 → 0으로
            // 진동 offset: 충격 직후 왼쪽으로 툵힜다는 느낌
            const float shakeX = std::sin(prop.pushFlashTimer * 60.f) * 4.f * t;
            Math::Matrix shakeModel = Math::Matrix::CreateTranslation({ worldC.x + shakeX, worldC.y })
                                    * Math::Matrix::CreateScale(prop.size);
            shader.setFloat("alpha", 1.0f);
            shader.setVec3("colorTint", 0.5f + 0.5f * t, 0.8f + 0.2f * t, 1.0f);
            shader.setFloat("tintStrength", t * 0.7f);
            m_tunnelObjectTex->Draw(shader, shakeModel);
            shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
            shader.setFloat("tintStrength", 0.0f);
        }
        else
        {
            shader.setFloat("alpha", 1.0f);
            shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
            shader.setFloat("tintStrength", 0.0f);
            m_tunnelObjectTex->Draw(shader, model);
        }
    }
}

void Train::CheatWarpToTunnelInside(Player& player, Math::Vec2 playerHitboxSize)
{
    m_car3InsideViewActive       = true;   // suppress exterior drones/robots
    m_car3TunnelInsideViewActive = true;
    m_trainState                 = TrainState::Stationary;
    m_trainCurrentSpeed          = 0.f;
    m_tunnelInsideWorldLeft = MIN_X;
    if (m_tunnelInsideTrain && m_tunnelInsideTrain->GetWidth() > 0)
        m_tunnelInsideWorldWidth = static_cast<float>(m_tunnelInsideTrain->GetWidth());
    InitTunnelInsideProps();
    SnapPlayerToTunnelInsideRail(player, playerHitboxSize);
    m_trainCarGapFalling = false;
    m_tunnelInsideHazardFalling = false;
    m_tunnelInsideHazardTimer   = 0.f;
    player.GetPulseCore().getPulse().set(player.GetPulseCore().getPulse().Max());
    if (m_sirenDroneManager)
        m_sirenDroneManager->ClearAllDrones();
}

void Train::CheatWarpToCar5(Player& player, Math::Vec2 playerHitboxSize)
{
    m_car3InsideViewActive       = false;
    m_car3TunnelInsideViewActive = false;
    m_trainCheatCarUnlock        = true;

    m_car3InsideTransitionActive = false;
    m_car3TunnelInsideTransitionActive = false;

    // Reset TunnelInside state variables
    m_tunnelInsideInjectT              = 0.f;
    m_tunnelInsideInjectComplete       = false;
    m_tunnelInsideDepartStarted        = false;
    m_tunnelInsideTrainVisualX         = 0.f;
    m_tunnelInsideDepartTrainOffset0   = 0.f;
    m_tunnelInsideBoardTimer           = -1.f;
    m_playerOnTunnelBoardingFloor        = false;
    m_playerOnTunnelDepartWalkFloor      = false;

    // Snapping player to the center of Car 5 (water tank car)
    const float cx = GetTrainCarCenterWorldX(5);
    const float deckSurfaceY = MIN_Y + kTrainFlatbedDeckTopLocalY;
    const float halfH = playerHitboxSize.y * 0.5f;

    const Math::Vec2 oldHb = player.GetHitboxCenter();
    const Math::Vec2 newHb = { cx, deckSurfaceY + halfH };

    player.SetPosition(player.GetPosition() + (newHb - oldHb));
    player.SetCurrentGroundLevel(deckSurfaceY);
    player.ResetVelocity();
    player.SetOnGround(true);

    m_tunnelInsideCameraSnapPending = true;

    // Guarantee train is moving at full speed
    m_trainState = TrainState::Moving;
    m_trainCurrentSpeed = TRAIN_SPEED;
    if (m_sirenDroneManager)
        m_sirenDroneManager->ClearAllDrones();
}
