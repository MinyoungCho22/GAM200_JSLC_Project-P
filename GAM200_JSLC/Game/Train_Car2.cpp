// Train_Car2.cpp - SecondTrain (Car2): purple pulse container

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

// 플레이어가 Car2 보라 컨테이너(Entering/Inside 상태) 안에 있는지 반환함
bool Train::IsPlayerInCar2PurplePulseBox(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize) const
{
    if (!m_car2PurpleHbValid)
        return false;
    // Entering / Inside 모두 컨테이너 안으로 스냅됨 — 추적·레이더 차단은 페이드 시작부터 적용
    if (m_car2HidePhase != Car2HidePhase::Inside && m_car2HidePhase != Car2HidePhase::Entering)
        return false;
    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };
    return Collision::CheckAABB(playerHbCenter, playerHitboxSize, contC, m_car2PurpleHb.size);
}


// 사이렌 드론 피해를 히딩 스팟 또는 보라 컨테이너로 막을 수 있는 상태인지 반환함
bool Train::IsSirenDroneDamageBlocked(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                                      bool isPlayerCrouching) const
{
    return IsPlayerHiding(playerHbCenter, playerHitboxSize, isPlayerCrouching)
        || IsPlayerInCar2PurplePulseBox(playerHbCenter, playerHitboxSize);
}


// 전달된 히트박스가 보라 컨테이너 히트박스와 동일한지 확인함 (숨기기 중 충돌 해제 판별용)
bool Train::IsCar2PurpleHitbox(const TrainHitbox& hb) const
{
    if (!m_car2PurpleHbValid)
        return false;
    constexpr float eps = 1.5f;
    return std::abs(hb.localCenter.x - m_car2PurpleHb.localCenter.x) < eps
        && std::abs(hb.localCenter.y - m_car2PurpleHb.localCenter.y) < eps
        && std::abs(hb.size.x - m_car2PurpleHb.size.x) < eps
        && std::abs(hb.size.y - m_car2PurpleHb.size.y) < eps;
}


// 보라 컨테이너의 진입(Entering)·내부(Inside)·퇴장 상태를 매 프레임 갱신함
// — 잠금 3초, 충전 100% 달성 후 A키로 퇴장 가능함
void Train::UpdateCar2PurpleContainer(float dt, Player& player, Math::Vec2 /*mouseWorldPos*/, bool /*attackTriggered*/,
                                      bool pulseAbsorbHeld, bool injectGodMode)
{
    if (!m_car2PurpleHbValid)
        return;

    constexpr float kLockDuration  = 3.0f;   // 진입 후 퇴장 잠금 시간
    constexpr float kEnterFadeDur  = 0.55f;  // 페이드인 시간
    constexpr float kInsideAlpha   = 0.38f;  // 안에 있을 때 투명도

    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 contC  = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };
    const float      boxL   = contC.x - m_car2PurpleHb.size.x * 0.5f;
    const float      boxR   = contC.x + m_car2PurpleHb.size.x * 0.5f;

    const Math::Vec2 pHb    = player.GetHitboxCenter();
    const Math::Vec2 pHalf  = player.GetHitboxSize() * 0.5f;
    const float      pL     = pHb.x - pHalf.x;
    const float      pR     = pHb.x + pHalf.x;

    const Math::Vec2 promptCenter = { boxL - 115.f, contC.y + 10.f };
    const Math::Vec2 promptSize   = { 150.f, 78.f };
    const bool        enterPromptOverlapped = Collision::CheckAABB(pHb, pHalf * 2.f, promptCenter, promptSize);

    // 진입 트리거는 Enter.png와 플레이어 히트박스가 겹칠 때만 허용한다.
    // 위/아래/오른쪽 경계에서는 진입하지 못하고, 왼쪽 경계만 풀린다.
    const bool nearEntry = enterPromptOverlapped;

    // 왼쪽으로 완전히 나갔는지 (Inside 상태 퇴장 조건)
    const bool exitedLeft = (pR <= boxL + 4.f);
    // Entering 시작 직후에는 플레이어가 아직 박스 왼쪽에 있으므로 exitedLeft가 참일 수 있다.
    // Enter.png보다 더 왼쪽으로 확실히 벗어난 경우에만 진입 취소로 본다.
    const bool cancelledEntryLeft = (pR < promptCenter.x - promptSize.x * 0.5f - 12.f);

    auto smoothstep = [](float t) -> float {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    };

    switch (m_car2HidePhase)
    {
    case Car2HidePhase::None:
        player.SetTrainCar2ForcedCrouch(false);
        player.SetMovementLockedByTrain(false);
        player.SetCar2LeaveWalk(false);
        player.SetTrainJumpBlocked(false);
        player.SetSpriteAlphaMul(1.f);
        m_car2InsideLockTimer = 0.f;

        // 재진입 쿨다운 차감
        if (m_car2ReEnterCooldown > 0.f)
            m_car2ReEnterCooldown = std::max(0.f, m_car2ReEnterCooldown - dt);

        // A/D 키로 걸어서 박스에 접근하면 자동 진입 시작 (쿨다운 중에는 진입 불가)
        if (nearEntry && m_car2InsideCharge < 99.5f && m_car2ReEnterCooldown <= 0.f)
        {
            // 덱 레벨에서 진입 시 — 컨테이너 바닥 높이(MIN_Y+561)로 플레이어 Y 스냅
            const float     kFloorTopWorldY = MIN_Y + 561.f;
            const float     currentFeet     = pHb.y - pHalf.y;
            if (currentFeet < kFloorTopWorldY - 4.f)
            {
                const float snapDy = kFloorTopWorldY - currentFeet;
                player.SetPosition(player.GetPosition() + Math::Vec2{ 0.f, snapDy });
                player.SetOnGround(true);
                player.ResetVelocity();
            }

            m_car2HidePhase   = Car2HidePhase::Entering;
            m_car2HideSeqTime = 0.f;
        }
        break;

    case Car2HidePhase::Entering:
    {
        // 이동 잠금 없음 — 플레이어가 자유롭게 걸어 들어감
        player.SetTrainCar2ForcedCrouch(false);
        player.SetMovementLockedByTrain(false);
        player.SetCar2LeaveWalk(false);
        player.SetTrainJumpBlocked(false);

        m_car2HideSeqTime += dt;
        const float u = smoothstep(std::min(1.f, m_car2HideSeqTime / kEnterFadeDur));
        player.SetSpriteAlphaMul(1.f - (1.f - kInsideAlpha) * u);

        // 페이드 완료 전에 Enter.png 영역보다 더 왼쪽으로 돌아가면 취소
        if (cancelledEntryLeft && m_car2HideSeqTime < kEnterFadeDur * 0.75f)
        {
            m_car2HidePhase = Car2HidePhase::None;
            player.SetSpriteAlphaMul(1.f);
            break;
        }

        if (m_car2HideSeqTime >= kEnterFadeDur)
        {
            m_car2HidePhase       = Car2HidePhase::Inside;
            m_car2HideSeqTime     = 0.f;
            m_car2InsideLockTimer = 0.f;
            player.SetTrainJumpBlocked(true);
            if (player.GetVelocity().y > 0.f)
                player.ResetVerticalVelocity();
            player.SetSpriteAlphaMul(kInsideAlpha);
        }
    }
    break;

    case Car2HidePhase::Inside:
    {
        // 이동 잠금 없음 — A/D로 박스 안에서 자유 이동
        player.SetMovementLockedByTrain(false);
        player.SetCar2LeaveWalk(false);
        player.SetTrainCar2ForcedCrouch(false);
        player.SetTrainJumpBlocked(true);
        if (player.GetVelocity().y > 0.f)
            player.ResetVerticalVelocity();
        player.SetSpriteAlphaMul(kInsideAlpha);

        // 잠금 타이머 증가
        m_car2InsideLockTimer = std::min(m_car2InsideLockTimer + dt, kLockDuration);

        // 펄스 충전
        float chargeMeterPerSec = 30.f;
        float pulsePerSec       = 40.f;
        if (pulseAbsorbHeld || injectGodMode)
        {
            chargeMeterPerSec = 48.f;
            pulsePerSec       = 72.f;
        }
        player.GetPulseCore().getPulse().add(pulsePerSec * static_cast<float>(dt));
        m_car2InsideCharge = std::min(100.f, m_car2InsideCharge + chargeMeterPerSec * static_cast<float>(dt));

        const bool exitAllowed = (m_car2InsideLockTimer >= kLockDuration) && (m_car2InsideCharge >= 99.5f);

        // 오른쪽 경계는 Inside 상태에서도 항상 유지한다.
        if (pR > boxR)
        {
            player.SetPosition(player.GetPosition() + Math::Vec2{ boxR - pR, 0.f });
            player.SetOnGround(true);
        }

        // 충전/잠금 중에는 왼쪽 경계를 유지해서 플레이어가 밖으로 나가지 못하게 함.
        if (!exitAllowed && pL < boxL)
        {
            player.SetPosition(player.GetPosition() + Math::Vec2{ boxL - pL, 0.f });
            player.SetOnGround(true);
        }

        // 퇴장: 충전 완료 + 잠금 해제 + A키로 왼쪽으로 걸어 나감
        if (exitAllowed && exitedLeft)
        {
            player.SetSpriteAlphaMul(1.f);
            player.SetMovementLockedByTrain(false);
            player.SetTrainJumpBlocked(false);
            m_car2HidePhase       = Car2HidePhase::None;
            m_car2InsideCharge    = 0.f;
            m_car2ReEnterCooldown = kLockDuration;  // 3초 쿨다운 후 재진입 가능
        }
    }
    break;
    }
}



// 컨테이너 왼쪽에 Enter·Leave 프롬프트 아이콘을 상태에 따라 그림
void Train::DrawCar2EnterLeavePrompt(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car2PurpleHbValid)
        return;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };

    const Math::Vec2 promptCenter = { contC.x - m_car2PurpleHb.size.x * 0.5f - 115.f, contC.y + 10.f };

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (promptCenter.x < visLeft || promptCenter.x > visRight)
        return;

    // Enter.png: None 상태에서 힌트용으로만 표시 (클릭 불필요, 걸어서 진입)
    // Leave.png: Inside 상태에서 충전 완료 + 잠금 해제 시 표시 (걸어서 왼쪽으로 나가라는 힌트)
    Background* tex = nullptr;
    if (m_car2HidePhase == Car2HidePhase::Inside
        && m_car2InsideCharge >= 99.5f && m_car2InsideLockTimer >= 3.f
        && m_car2LeavePromptTex && m_car2LeavePromptTex->GetWidth() > 0)
        tex = m_car2LeavePromptTex.get();
    else if (m_car2HidePhase == Car2HidePhase::None
        && m_car2InsideCharge < 99.5f && m_car2ReEnterCooldown <= 0.f
        && m_car2EnterPromptTex && m_car2EnterPromptTex->GetWidth() > 0)
        tex = m_car2EnterPromptTex.get();

    if (!tex)
        return;

    const float pw = static_cast<float>(tex->GetWidth());
    const float ph = static_cast<float>(tex->GetHeight());

    textureShader.use();
    textureShader.setBool("flipX", false);
    textureShader.setFloat("alpha", 1.0f);
    textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    textureShader.setFloat("tintStrength", 0.0f);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);

    Math::Matrix model =
        Math::Matrix::CreateTranslation(promptCenter) * Math::Matrix::CreateScale({ pw * 0.85f, ph * 0.85f });
    tex->Draw(textureShader, model);
}


// 진입 잠금 또는 재진입 쿨다운 남은 시간을 컨테이너 왼쪽에 세로 바로 그림
void Train::DrawCar2InsideLockTimer(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car2PurpleHbValid || !m_skyVAO)
        return;

    constexpr float kLockDuration = 3.0f;
    float           remaining     = 0.f;

    if (m_car2HidePhase == Car2HidePhase::Inside)
        remaining = std::max(0.f, kLockDuration - m_car2InsideLockTimer);
    else if (m_car2HidePhase == Car2HidePhase::None)
        remaining = std::max(0.f, m_car2ReEnterCooldown);

    if (remaining <= 0.f)
        return; // 잠금/쿨다운이 없으면 표시 안 함

    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 contC  = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (contC.x < visLeft || contC.x > visRight)
        return;

    // 박스 왼쪽 가장자리 바깥에 세로 바를 그림 (PulseSource 게이지와 유사한 스타일)
    constexpr float kBarW      = 10.f;
    constexpr float kBarH      = 80.f;
    constexpr float kOffsetX   = -38.f; // 박스 왼쪽에서 왼쪽으로

    const float barCenterX = contC.x - m_car2PurpleHb.size.x * 0.5f + kOffsetX;
    const float barBotY    = contC.y - kBarH * 0.5f;
    const float fillFrac   = std::clamp(remaining / kLockDuration, 0.f, 1.f); // 1→0 (남은 시간 비율)

    // 배경 (어두운 보라)
    DrawFilledQuad(colorShader,
        { barCenterX, contC.y },
        { kBarW, kBarH },
        0.25f, 0.05f, 0.35f, 0.85f);

    // 채움 바 (밝은 보라, 위에서 채움)
    const float fillH       = kBarH * fillFrac;
    const float fillCenterY = barBotY + kBarH - fillH * 0.5f;
    if (fillH > 0.5f)
    {
        DrawFilledQuad(colorShader,
            { barCenterX, fillCenterY },
            { kBarW - 2.f, fillH - 1.f },
            0.72f, 0.32f, 1.0f, 0.95f);
    }

    // 테두리
    DrawFilledQuad(colorShader,
        { barCenterX, contC.y },
        { kBarW + 2.f, kBarH + 2.f },
        0.55f, 0.20f, 0.80f, 0.55f);
}


// 마우스 커서가 Enter·Leave 아이콘 위에 있고 플레이어가 컨테이너 근처인지 반환함
bool Train::IsCar2EnterLeavePromptHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld,
                                         Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car2PurpleHbValid)
        return false;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };

    const bool nearPurple = Collision::CheckAABB(playerHbCenter, playerHbSize, contC,
                                                   { m_car2PurpleHb.size.x + 520.f, m_car2PurpleHb.size.y + 520.f });

    const Math::Vec2 promptCenter = { contC.x - m_car2PurpleHb.size.x * 0.5f - 115.f, contC.y + 10.f };
    const Math::Vec2 promptSize   = { 150.f, 78.f };

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (promptCenter.x < visLeft || promptCenter.x > visRight)
        return false;

    return nearPurple && Collision::CheckPointInAABB(mouseWorld, promptCenter, promptSize);
}

