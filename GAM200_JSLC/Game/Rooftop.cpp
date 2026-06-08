// Rooftop.cpp

#include "Rooftop.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "MapObjectConfig.hpp"
#include "../Game/PulseCore.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"

#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// [Initialize]
// - 기능: Rooftop 맵의 배경 텍스처, 리프트 엘리베이터 모델 및 감지 드론 5마리를 기본 스폰하여 초기화합니다.
// - 가이드라인: 리프트의 초기 위치 및 물리 좌표계 계산, 우측 목표 착지 좌표(`m_liftTargetX`)를 상수 기준으로 할당합니다.
// ---------------------------------------------------------------------------
void Rooftop::Initialize()
{
    // Initialize background assets for different states
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Rooftop.png");
    m_closeBackground = std::make_unique<Background>();
    m_closeBackground->Initialize("Asset/Rooftop_Close.png");

    m_light = std::make_unique<Background>();
    m_light->Initialize("Asset/Rooftop_Light.png");

    // Initialize the lift platform
    m_lift = std::make_unique<Background>();
    m_lift->Initialize("Asset/Lift.png");
    m_holeSprite = std::make_unique<Background>();
    m_liftButtonSprite = std::make_unique<Background>();

    // Define lift dimensions and initial position
    float liftWidth = 351.f;
    float liftHeight = 345.f;
    float liftTopLeftX = 13745.f;
    float liftTopY = 1799.f;

    // Convert top-left coordinates to center-based coordinates
    m_liftPos = { liftTopLeftX + (liftWidth / 2.0f), liftTopY - (liftHeight / 2.0f) };
    m_liftSize = { liftWidth, liftHeight };
    m_liftInitialX = m_liftPos.x;

    // Define the destination target for the lift
    float targetLeftEdgeX = 14928.0f;
    m_liftTargetX = targetLeftEdgeX + (liftWidth / 2.0f);

    // Initialize lift state and variables
    m_liftState = LiftState::Idle;
    m_isPlayerNearLift = false;
    m_liftSpeed = 250.0f;
    m_liftCountdown = 3.0f;
    m_isLiftActivated = false;
    m_isLiftGapUnlocked = false;
    m_hasPrevPlayerX = false;
    m_prevPlayerX = 0.0f;
    m_liftDirection = 1.0f;

    // Initialize rooftop level dimensions
    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    // Initialize and spawn drones at specific level coordinates
    m_droneManager = std::make_unique<DroneManager>();
    m_droneManager->SpawnDrone({ 8700.0f, 1700.0f }, "Asset/Drone.png", false);
    m_droneManager->SpawnDrone({ 13500.0f, 1900.0f }, "Asset/Drone.png", true);
    m_droneManager->SpawnDrone({ 14500.0f, 1750.0f }, "Asset/Drone.png", true);
    m_droneManager->SpawnDrone({ 15500.0f, 1700.0f }, "Asset/Drone.png", false);
    m_droneManager->SpawnDrone({ 17250.0f, 1800.0f }, "Asset/Drone.png", true);

    ApplyConfig(MapObjectConfig::Instance().GetData().rooftop);

    m_isClose = false;
    m_isPlayerClose = false;
}

// ---------------------------------------------------------------------------
// [ApplyConfig]
// - 기능: JSON 데이터 구조체로부터 리프트 버튼 위치, 낙하 구멍(Hole) 위치 및 충격 펄스 충전기들의 사양을 옥상 맵에 반영합니다.
// - 매개변수:
//   - cfg: Rooftop 맵 정보가 담긴 설정 레퍼런스
// ---------------------------------------------------------------------------
void Rooftop::ApplyConfig(const RooftopObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    m_pulseSources.clear();

    m_debugBoxPos = { cfg.hole.topLeft.x + cfg.hole.size.x * 0.5f,
                      cfg.hole.topLeft.y - cfg.hole.size.y * 0.5f };
    m_debugBoxSize = cfg.hole.size;
    if (m_holeSprite && !cfg.hole.spritePath.empty())
    {
        m_holeSprite->Shutdown();
        m_holeSprite->Initialize(cfg.hole.spritePath.c_str());
    }

    for (const auto& p : cfg.pulseSources)
    {
        Math::Vec2 center = { p.topLeft.x + p.size.x * 0.5f, p.topLeft.y - p.size.y * 0.5f };
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize(center, p.size, 100.0f);
        if (!p.spritePath.empty()) m_pulseSources.back().InitializeSprite(p.spritePath.c_str());
    }

    if (m_liftButtonSprite && !cfg.liftButton.spritePath.empty())
    {
        m_liftButtonSprite->Shutdown();
        m_liftButtonSprite->Initialize(cfg.liftButton.spritePath.c_str());
    }
    float w = static_cast<float>(m_liftButtonSprite ? m_liftButtonSprite->GetWidth() : 0);
    float h = static_cast<float>(m_liftButtonSprite ? m_liftButtonSprite->GetHeight() : 0);
    if (w <= 0.0f) w = (cfg.liftButton.fallbackSize.x > 0.0f ? cfg.liftButton.fallbackSize.x : 320.0f);
    if (h <= 0.0f) h = (cfg.liftButton.fallbackSize.y > 0.0f ? cfg.liftButton.fallbackSize.y : 96.0f);
    m_liftButtonSize = { w, h };
    m_liftButtonPos = { cfg.liftButton.topLeft.x + w * 0.5f, cfg.liftButton.topLeft.y - h * 0.5f };
}

// ---------------------------------------------------------------------------
// [SyncGroundLevelForPlayer]
// - 기능: 움직이는 리프트 발판 위나 맵 바닥면 위에 서있는 플레이어의 물리 착지 기준 높이(Ground Level)를 계산하여 강제 동기화합니다.
// - 매개변수:
//   - player: 플레이어 객체 레퍼런스
//   - playerHitboxSize: 플레이어 히트박스 크기
// - 가이드라인: 리프트가 있는 낭떠러지(Abyss) 구간에서는 리프트를 밟고 있거나 기동 대기 중인 상태에서만 발판 높이로 동기화하며, 공중 낙하 시에는 지옥 낙하 처리를 위해 바닥 제한을 해제(abyssGroundY)합니다.
// ---------------------------------------------------------------------------
void Rooftop::SyncGroundLevelForPlayer(Player& player, Math::Vec2 playerHitboxSize)
{
    Math::Vec2 playerPos = player.GetPosition();
    bool isPlayerInRooftop = (playerPos.y >= MIN_Y - 1000.0f && playerPos.y <= MIN_Y + HEIGHT &&
        playerPos.x >= MIN_X && playerPos.x <= MIN_X + WIDTH);
    if (!isPlayerInRooftop)
        return;

    Math::Vec2 liftHalfSizeCheck = m_liftSize / 2.0f;
    Math::Vec2 liftMinCheck = m_liftPos - liftHalfSizeCheck;
    Math::Vec2 liftMaxCheck = m_liftPos + liftHalfSizeCheck;

    Math::Vec2 playerHalfCheck = playerHitboxSize / 2.0f;
    Math::Vec2 playerMinCheck = playerPos - playerHalfCheck;
    Math::Vec2 playerMaxCheck = playerPos + playerHalfCheck;

    bool isTouchingLift = (playerMaxCheck.x > liftMinCheck.x && playerMinCheck.x < liftMaxCheck.x &&
        playerMaxCheck.y > liftMinCheck.y && playerMinCheck.y < liftMaxCheck.y);
    const float deckY = liftMinCheck.y + LIFT_DECK_SURFACE_OFFSET_FROM_MIN_Y;
    const float playerFootY = playerPos.y - playerHalfCheck.y;
    const bool horizontallyOnLift = (playerMaxCheck.x > liftMinCheck.x && playerMinCheck.x < liftMaxCheck.x);
    const bool nearLiftDeck = std::fabs(playerFootY - deckY) <= 40.0f;
    const bool approachingDeckFromAbove = playerFootY >= deckY - 20.0f;
    const bool isRidingLiftNow = isTouchingLift && horizontallyOnLift && nearLiftDeck &&
        approachingDeckFromAbove && player.GetVelocity().y <= 0.0f;

    const float safeGroundY = FLOOR_SURFACE_Y;
    const float abyssGroundY = MIN_Y - 800.0f;
    const float abyssStartX = m_liftInitialX - (m_liftSize.x / 2.0f) + 90.0f;
    const float abyssEndX = m_liftTargetX + (m_liftSize.x / 2.0f) + 50.0f;

    if (playerPos.x > abyssStartX && playerPos.x < abyssEndX)
    {
        // Gap area: the lift deck is safe while standing on it, including countdown before motion.
        if (isRidingLiftNow || (m_isLiftGapUnlocked && isTouchingLift && horizontallyOnLift &&
            nearLiftDeck && approachingDeckFromAbove))
        {
            player.SetCurrentGroundLevel(std::max(safeGroundY, deckY));
        }
        else
        {
            player.SetCurrentGroundLevel(abyssGroundY);
            if (playerPos.y > abyssGroundY + 50.0f)
                player.SetOnGround(false);
        }
    }
    else
    {
        player.SetCurrentGroundLevel(safeGroundY);
    }
}

// ---------------------------------------------------------------------------
// [Update]
// - 기능: 리프트 작동 카운트다운/이동 처리, 리프트 위 플레이어 관성 캐리, 낙하 구멍 메우기 퍼즐 상호작용 및 외벽 충돌을 업데이트합니다.
// - 매개변수:
//   - dt: 프레임 시간 델타
//   - player: 플레이어 레퍼런스
//   - playerHitboxSize: 플레이어 히트박스 크기
//   - input: 입력 상태 관리 객체 레퍼런스
//   - mouseWorldPos: 마우스의 월드 좌표
//   - isLeftClickTriggered: 마우스 좌클릭 트리거 여부
// - 가이드라인:
//   - 낙하 구멍이 열려 있는 동안(`!m_isClose`) 플레이어가 구멍의 좌우 X 영역을 더블 점프 등으로 넘어가지 못하도록 수직 경계벽 충돌 처리를 엄격히 적용해야 합니다.
//   - 리프트 버튼 작동 비용은 8.0f이며, 플레이어가 작동 버튼 근처에서 마우스를 클릭해야 합니다.
// ---------------------------------------------------------------------------
void Rooftop::Update(double dt, Player& player, Math::Vec2 playerHitboxSize, Input::Input& input,
                     Math::Vec2 mouseWorldPos, bool isLeftClickTriggered)
{
    float oldLiftX = m_liftPos.x;
    Math::Vec2 playerPos = player.GetPosition();
    float prevPlayerX = m_hasPrevPlayerX ? m_prevPlayerX : playerPos.x;
    auto clampf = [](float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); };

    bool isPlayerInRooftop = (playerPos.y >= MIN_Y - 1000.0f && playerPos.y <= MIN_Y + HEIGHT &&
        playerPos.x >= MIN_X && playerPos.x <= MIN_X + WIDTH);

    if (!isPlayerInRooftop)
    {
        return;
    }

    {
        Math::Vec2 phsWall = playerHitboxSize / 2.0f;
        if (playerPos.x - phsWall.x < SPAWN_LEFT_WALL_FACE_X)
        {
            Math::Vec2 clamped = playerPos;
            clamped.x = SPAWN_LEFT_WALL_FACE_X + phsWall.x;
            player.SetPosition(clamped);
            playerPos = clamped;
        }
    }

    const float abyssStartX = m_liftInitialX - (m_liftSize.x / 2.0f) + 90.0f;
    const float abyssEndX = m_liftTargetX + (m_liftSize.x / 2.0f) + 50.0f;

    Math::Vec2 liftHalfForAbyss = m_liftSize / 2.0f;
    Math::Vec2 liftMinForAbyss = m_liftPos - liftHalfForAbyss;
    Math::Vec2 liftMaxForAbyss = m_liftPos + liftHalfForAbyss;
    Math::Vec2 playerHalfForLift = playerHitboxSize / 2.0f;
    Math::Vec2 playerMinForLift = playerPos - playerHalfForLift;
    Math::Vec2 playerMaxForLift = playerPos + playerHalfForLift;
    bool isTouchingLift = (playerMaxForLift.x > liftMinForAbyss.x && playerMinForLift.x < liftMaxForAbyss.x &&
        playerMaxForLift.y > liftMinForAbyss.y && playerMinForLift.y < liftMaxForAbyss.y);

    // Hole Interaction Logic: Check if player is close to the interactable rooftop hole
    {
        Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        Math::Vec2 boxHalfSize = m_debugBoxSize / 2.0f;
        Math::Vec2 boxMin = m_debugBoxPos - boxHalfSize;
        Math::Vec2 boxMax = m_debugBoxPos + boxHalfSize;
        
        Math::Vec2 closestPointOnBox;
        closestPointOnBox.x = clampf(playerPos.x, boxMin.x, boxMax.x);
        closestPointOnBox.y = clampf(playerPos.y, boxMin.y, boxMax.y);

        Math::Vec2 closestPointOnPlayer;
        closestPointOnPlayer.x = clampf(closestPointOnBox.x, playerMin.x, playerMax.x);
        closestPointOnPlayer.y = clampf(closestPointOnBox.y, playerMin.y, playerMax.y);

        float distanceSq = (closestPointOnPlayer - closestPointOnBox).LengthSq();
        const float PROXIMITY_RANGE_SQ = 100.0f * 100.0f;

        m_isPlayerClose = (distanceSq <= PROXIMITY_RANGE_SQ);

        // Interact to close the hole using energy
        if (m_isPlayerClose && isLeftClickTriggered && !m_isClose)
        {
            const float INTERACT_COST = 5.0f;
            Pulse& pulse = player.GetPulseCore().getPulse();

            if (pulse.Value() >= INTERACT_COST)
            {
                pulse.spend(INTERACT_COST);
                m_isClose = true;
            }
        }
    }

    // Lift Button Proximity Logic: check if player is close enough to interact with LiftBotton sprite area
    {
        Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        Math::Vec2 buttonHalfSize = m_liftButtonSize / 2.0f;
        Math::Vec2 buttonMin = m_liftButtonPos - buttonHalfSize;
        Math::Vec2 buttonMax = m_liftButtonPos + buttonHalfSize;

        Math::Vec2 closestPointOnButton;
        closestPointOnButton.x = clampf(playerPos.x, buttonMin.x, buttonMax.x);
        closestPointOnButton.y = clampf(playerPos.y, buttonMin.y, buttonMax.y);

        Math::Vec2 closestPointOnPlayer;
        closestPointOnPlayer.x = clampf(closestPointOnButton.x, playerMin.x, playerMax.x);
        closestPointOnPlayer.y = clampf(closestPointOnButton.y, playerMin.y, playerMax.y);

        float distanceSq = (closestPointOnPlayer - closestPointOnButton).LengthSq();
        const float LIFT_BUTTON_PROXIMITY_RANGE_SQ = 150.0f * 150.0f;

        m_isPlayerNearLift = (distanceSq <= LIFT_BUTTON_PROXIMITY_RANGE_SQ) && (m_liftState == LiftState::Idle);
    }

    bool clickedLiftButton = false;
    if (isLeftClickTriggered)
    {
        Math::Vec2 buttonHalf = m_liftButtonSize * 0.5f;
        Math::Vec2 buttonMin = m_liftButtonPos - buttonHalf;
        Math::Vec2 buttonMax = m_liftButtonPos + buttonHalf;
        clickedLiftButton = (mouseWorldPos.x >= buttonMin.x && mouseWorldPos.x <= buttonMax.x &&
                             mouseWorldPos.y >= buttonMin.y && mouseWorldPos.y <= buttonMax.y);
    }

    // Lift state machine
    if (m_liftState == LiftState::Idle)
    {
        // Activate only when player is near and clicks on the button sprite area
        if (m_isPlayerNearLift && clickedLiftButton)
        {
            const float LIFT_COST = 8.0f;
            Pulse& pulse = player.GetPulseCore().getPulse();

            if (pulse.Value() >= LIFT_COST)
            {
                pulse.spend(LIFT_COST);
                m_liftState = LiftState::Countdown;
                m_liftCountdown = 3.0f;
                m_isLiftActivated = false;
                m_isLiftGapUnlocked = true;

                // Determine movement direction based on current position relative to start/target
                if (m_liftPos.x < (m_liftInitialX + m_liftTargetX) / 2.0f)
                {
                    m_liftDirection = 1.0f;
                }
                else
                {
                    m_liftDirection = -1.0f;
                }

                Logger::Instance().Log(Logger::Severity::Event, "Lift activated! Countdown started.");
            }
        }
    }
    else if (m_liftState == LiftState::Countdown)
    {
        m_liftCountdown -= static_cast<float>(dt);

        if (m_liftCountdown <= 0.0f)
        {
            m_liftState = LiftState::Moving;
            Logger::Instance().Log(Logger::Severity::Event, "Lift moving!");
        }
    }
    else if (m_liftState == LiftState::Moving)
    {
        m_liftPos.x += m_liftSpeed * m_liftDirection * static_cast<float>(dt);

        // Check if lift reached the right target
        if (m_liftDirection > 0.0f)
        {
            if (m_liftPos.x >= m_liftTargetX)
            {
                m_liftPos.x = m_liftTargetX;
                m_liftState = LiftState::AtDestination;
            }
        }
        // Check if lift reached the left initial point
        else
        {
            if (m_liftPos.x <= m_liftInitialX)
            {
                m_liftPos.x = m_liftInitialX;
                m_liftState = LiftState::AtDestination;
            }
        }
    }
    else if (m_liftState == LiftState::AtDestination)
    {
        m_liftState = LiftState::Idle;
    }

    // Moving Platform Physics: Move the player horizontally if they are standing on the lift
    float deltaX = m_liftPos.x - oldLiftX;
    {
        Math::Vec2 liftHalfSize = m_liftSize * 0.5f;
        Math::Vec2 liftMin = m_liftPos - liftHalfSize;
        Math::Vec2 liftMax = m_liftPos + liftHalfSize;

        Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;
        Math::Vec2 livePlayerPos = player.GetPosition();
        Math::Vec2 playerMin = livePlayerPos - playerHalfSize;
        Math::Vec2 playerMax = livePlayerPos + playerHalfSize;

        const bool overlapsLift = (playerMax.x > liftMin.x && playerMin.x < liftMax.x &&
            playerMax.y > liftMin.y && playerMin.y < liftMax.y);
        const float deckY = liftMin.y + LIFT_DECK_SURFACE_OFFSET_FROM_MIN_Y;
        const float playerFootY = livePlayerPos.y - playerHalfSize.y;
        const bool nearLiftDeck = std::fabs(playerFootY - deckY) <= 40.0f;
        m_isLiftActivated = overlapsLift && nearLiftDeck && player.GetVelocity().y <= 0.0f;

        if (m_isLiftActivated)
        {
            player.SetPosition({ player.GetPosition().x + deltaX, player.GetPosition().y });
        }
    }

    Math::Vec2 currentPlayerPos = player.GetPosition();
    Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;

    // Constrain player within the left boundary of the level
    float leftBoundary = MIN_X;
    if (currentPlayerPos.x - playerHalfSize.x < leftBoundary)
    {
        player.SetPosition({ leftBoundary + playerHalfSize.x, currentPlayerPos.y });
    }

    // Constrain player within the right boundary of the level
    float rightBoundary = MIN_X + WIDTH;
    if (currentPlayerPos.x + playerHalfSize.x > rightBoundary)
    {
        player.SetPosition({ rightBoundary - playerHalfSize.x, currentPlayerPos.y });
    }

    currentPlayerPos = player.GetPosition();

    // Force hole filling progression:
    // while the rooftop hole is open, block crossing its X-range entirely
    // (even by jumping over), so the player must close the hole first.
    // Behave like a static boundary wall (no knockback): keep player touching
    // the hole edge. Once hole is closed (m_isClose), this logic is skipped.
    bool blockedByHoleWall = false;
    if (!m_isClose)
    {
        Math::Vec2 holeHalfSize = m_debugBoxSize * 0.5f;
        float holeMinX = m_debugBoxPos.x - holeHalfSize.x;
        float holeMaxX = m_debugBoxPos.x + holeHalfSize.x;

        Math::Vec2 pos = currentPlayerPos;
        if (pos.x >= holeMinX - playerHalfSize.x && pos.x < m_debugBoxPos.x)
        {
            pos.x = holeMinX - playerHalfSize.x;
            blockedByHoleWall = true;
        }
        else if (pos.x <= holeMaxX + playerHalfSize.x && pos.x >= m_debugBoxPos.x)
        {
            pos.x = holeMaxX + playerHalfSize.x;
            blockedByHoleWall = true;
        }

        if (blockedByHoleWall)
        {
            player.SetPosition(pos);
            currentPlayerPos = pos;
        }
    }

    // Prevent bypassing the lift gap by jumping over it.
    // Crossing the gap is allowed only while physically on the lift.
    bool blockedByLiftGapWall = false;
    if (!m_isLiftGapUnlocked)
    {
        Math::Vec2 liftHalf = m_liftSize * 0.5f;
        Math::Vec2 liftMinNow = m_liftPos - liftHalf;
        Math::Vec2 liftMaxNow = m_liftPos + liftHalf;

        Math::Vec2 curPlayerMin = currentPlayerPos - playerHalfSize;
        Math::Vec2 curPlayerMax = currentPlayerPos + playerHalfSize;
        bool overlapsLiftNow = (curPlayerMax.x > liftMinNow.x && curPlayerMin.x < liftMaxNow.x &&
                                curPlayerMax.y > liftMinNow.y && curPlayerMin.y < liftMaxNow.y);
        float playerFootY = currentPlayerPos.y - playerHalfSize.y;
        const float liftDeckY = liftMinNow.y + LIFT_DECK_SURFACE_OFFSET_FROM_MIN_Y;
        bool nearLiftDeck = std::fabs(playerFootY - liftDeckY) <= 40.0f;
        bool isRidingLiftNow = overlapsLiftNow && nearLiftDeck && player.GetVelocity().y <= 0.0f;

        if (!isRidingLiftNow)
        {
            const float leftWallX = abyssStartX - playerHalfSize.x;
            const float rightWallX = abyssEndX + playerHalfSize.x;
            const float wallMidX = (leftWallX + rightWallX) * 0.5f;
            Math::Vec2 pos = currentPlayerPos;

            // Use previous frame X to prevent jump/dash tunneling through the wall.
            bool wasOnLeftSide = (prevPlayerX <= leftWallX);
            bool wasOnRightSide = (prevPlayerX >= rightWallX);
            bool isInsideGapBand = (pos.x > leftWallX && pos.x < rightWallX);
            bool crossedLeftToRight = (wasOnLeftSide && pos.x >= rightWallX);
            bool crossedRightToLeft = (wasOnRightSide && pos.x <= leftWallX);

            if (isInsideGapBand || crossedLeftToRight || crossedRightToLeft)
            {
                if (wasOnLeftSide || crossedLeftToRight)
                {
                    pos.x = leftWallX;
                }
                else if (wasOnRightSide || crossedRightToLeft)
                {
                    pos.x = rightWallX;
                }
                else
                {
                    pos.x = (pos.x < wallMidX) ? leftWallX : rightWallX;
                }
                blockedByLiftGapWall = true;
            }

            if (blockedByLiftGapWall)
            {
                player.SetPosition(pos);
                currentPlayerPos = pos;
            }
        }
    }

    // AABB Collision with the rooftop hole (only if it is not yet closed)
    if (!m_isClose && !blockedByHoleWall && !blockedByLiftGapWall)
    {
        Math::Vec2 holeHalfSize = m_debugBoxSize * 0.5f;
        Math::Vec2 holeMin = m_debugBoxPos - holeHalfSize;
        Math::Vec2 holeMax = m_debugBoxPos + holeHalfSize;

        Math::Vec2 playerMin = currentPlayerPos - playerHalfSize;
        Math::Vec2 playerMax = currentPlayerPos + playerHalfSize;

        if (playerMax.x > holeMin.x && playerMin.x < holeMax.x &&
            playerMax.y > holeMin.y && playerMin.y < holeMax.y)
        {
            // Resolve collision by finding the minimum overlap axis
            float overlapLeft = playerMax.x - holeMin.x;
            float overlapRight = holeMax.x - playerMin.x;
            float overlapTop = playerMax.y - holeMin.y;
            float overlapBottom = holeMax.y - playerMin.y;

            float minOverlap = std::min({ overlapLeft, overlapRight, overlapTop, overlapBottom });

            if (minOverlap == overlapLeft)
            {
                player.SetPosition({ holeMin.x - playerHalfSize.x, currentPlayerPos.y });
            }
            else if (minOverlap == overlapRight)
            {
                player.SetPosition({ holeMax.x + playerHalfSize.x, currentPlayerPos.y });
            }
            else if (minOverlap == overlapTop)
            {
                player.SetPosition({ currentPlayerPos.x, holeMin.y - playerHalfSize.y });
            }
            else if (minOverlap == overlapBottom)
            {
                player.SetPosition({ currentPlayerPos.x, holeMax.y + playerHalfSize.y });
            }
        }
    }

    // AABB Collision with the lift platform.
    // After the player presses the button, the countdown should let the player walk onto the lift
    // instead of getting blocked by the lift body before boarding.
    const bool allowBoardingDuringCountdown = m_isLiftGapUnlocked && m_liftState == LiftState::Countdown;
    if (!m_isLiftActivated && m_liftState != LiftState::AtDestination && !allowBoardingDuringCountdown)
    {
        Math::Vec2 liftHalfSize = m_liftSize * 0.5f;
        Math::Vec2 liftMin = m_liftPos - liftHalfSize;
        Math::Vec2 liftMax = m_liftPos + liftHalfSize;

        Math::Vec2 playerMin = currentPlayerPos - playerHalfSize;
        Math::Vec2 playerMax = currentPlayerPos + playerHalfSize;

        if (playerMax.x > liftMin.x && playerMin.x < liftMax.x &&
            playerMax.y > liftMin.y && playerMin.y < liftMax.y)
        {
            float overlapLeft = playerMax.x - liftMin.x;
            float overlapRight = liftMax.x - playerMin.x;
            float overlapTop = playerMax.y - liftMin.y;
            float overlapBottom = liftMax.y - playerMin.y;

            const float deckY = liftMin.y + LIFT_DECK_SURFACE_OFFSET_FROM_MIN_Y;
            const float playerFootY = currentPlayerPos.y - playerHalfSize.y;
            const bool canLandOnDeck = (player.GetVelocity().y <= 0.0f) && (playerFootY >= deckY - 30.0f);
            if (!canLandOnDeck)
            {
                overlapBottom = 1e9f;
            }

            // Before lift unlock, do not allow "landing on lift top" from above.
            // This prevents double-jump + drift bypass over the virtual gap walls.
            if (!m_isLiftGapUnlocked)
            {
                overlapBottom = 1e9f;
            }

            float minOverlap = std::min({ overlapLeft, overlapRight, overlapTop, overlapBottom });

            if (minOverlap == overlapLeft)
            {
                player.SetPosition({ liftMin.x - playerHalfSize.x, currentPlayerPos.y });
            }
            else if (minOverlap == overlapRight)
            {
                player.SetPosition({ liftMax.x + playerHalfSize.x, currentPlayerPos.y });
            }
            else if (minOverlap == overlapTop)
            {
                player.SetPosition({ currentPlayerPos.x, liftMin.y - playerHalfSize.y });
            }
            else if (minOverlap == overlapBottom)
            {
                const float deckY = liftMin.y + LIFT_DECK_SURFACE_OFFSET_FROM_MIN_Y;
                player.SetPosition({ currentPlayerPos.x, deckY + playerHalfSize.y });
            }

            currentPlayerPos = player.GetPosition();
        }
    }

    // Deactivate lift follow state if player leaves the platform
    if (m_liftState == LiftState::Idle && m_isLiftActivated)
    {
        Math::Vec2 liftHalfSize = m_liftSize * 0.5f;
        Math::Vec2 liftMin = m_liftPos - liftHalfSize;
        Math::Vec2 liftMax = m_liftPos + liftHalfSize;

        Math::Vec2 playerMin = currentPlayerPos - playerHalfSize;
        Math::Vec2 playerMax = currentPlayerPos + playerHalfSize;

        bool isPlayerStillOnLift = (playerMax.x > liftMin.x && playerMin.x < liftMax.x &&
            playerMax.y > liftMin.y && playerMin.y < liftMax.y);

        if (!isPlayerStillOnLift)
        {
            m_isLiftActivated = false;
        }
    }

    // Hazard: If player falls below the lift level into the gap, drain pulse energy
    if (currentPlayerPos.x > abyssStartX && currentPlayerPos.x < abyssEndX)
    {
        if (!isTouchingLift && currentPlayerPos.y < 1500.0f)
        {
            const float DEATH_DRAIN_RATE = 100.0f;
            player.GetPulseCore().getPulse().spend(DEATH_DRAIN_RATE * static_cast<float>(dt));

            Logger::Instance().Log(Logger::Severity::Event, "Falling into abyss! Pulse draining...");
        }
    }

    // Update enemies (drones)
    m_droneManager->Update(dt, player, playerHitboxSize, false, true, 1.f);

    m_prevPlayerX = player.GetPosition().x;
    m_hasPrevPlayerX = true;
}

// ---------------------------------------------------------------------------
// [Draw]
// - 기능: 옥상 배경 이미지, 닫힌 구멍 패치, 엘리베이터 버튼, 펄스 소스 충전소 및 움직이는 리프트 본체를 렌더링합니다.
// - 매개변수:
//   - shader: 렌더링 스프라이트용 기본 텍스처 셰이더 레퍼런스
// ---------------------------------------------------------------------------
void Rooftop::Draw(Shader& shader) const
{
    // Render current background (dark or closed-hole version)
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);

    if (m_isClose)
    {
        m_closeBackground->Draw(shader, model);
        m_light->Draw(shader, model);
    }
    else
    {
        m_background->Draw(shader, model);
        m_light->Draw(shader, model);
    }

    // Hole sprite: m_isClose(true) once player fills pulse and closes hole.
    // Collision/hitbox uses same center/size (m_debugBoxPos/m_debugBoxSize).
    if (!m_isClose && m_holeSprite)
    {
        Math::Matrix holeModel = Math::Matrix::CreateTranslation(m_debugBoxPos) * Math::Matrix::CreateScale(m_debugBoxSize);
        shader.setMat4("model", holeModel);
        m_holeSprite->Draw(shader, holeModel);
    }

    if (m_liftButtonSprite)
    {
        Math::Matrix buttonModel = Math::Matrix::CreateTranslation(m_liftButtonPos) * Math::Matrix::CreateScale(m_liftButtonSize);
        shader.setMat4("model", buttonModel);
        m_liftButtonSprite->Draw(shader, buttonModel);
    }

    for (const auto& source : m_pulseSources)
    {
        source.DrawSprite(shader);
    }

    // Render the lift platform
    Math::Matrix liftModel = Math::Matrix::CreateTranslation(m_liftPos) * Math::Matrix::CreateScale(m_liftSize);
    shader.setMat4("model", liftModel);
    m_lift->Draw(shader, liftModel);
}

// ---------------------------------------------------------------------------
// [DrawDrones]
// - 기능: 옥상 순찰 드론들의 비행 모션을 렌더링합니다.
// ---------------------------------------------------------------------------
void Rooftop::DrawDrones(Shader& shader) const
{
    m_droneManager->Draw(shader);
}

// ---------------------------------------------------------------------------
// [DrawRadars]
// - 기능: 드론 감지 레이더 원들을 렌더링합니다.
// ---------------------------------------------------------------------------
void Rooftop::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Render enemy detection radars
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

// ---------------------------------------------------------------------------
// [DrawGauges]
// - 기능: 드론 상단 경고 게이지 바를 그립니다.
// ---------------------------------------------------------------------------
void Rooftop::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Render enemy health/status gauges
    m_droneManager->DrawGauges(colorShader, debugRenderer);
}

// ---------------------------------------------------------------------------
// [Shutdown]
// - 기능: Rooftop 맵의 할당된 텍스처 및 드론들을 해제하고 메모리에서 소멸시킵니다.
// ---------------------------------------------------------------------------
void Rooftop::Shutdown()
{
    // Cleanup allocated resources
    if (m_background)
    {
        m_background->Shutdown();
    }
    if (m_closeBackground)
    {
        m_closeBackground->Shutdown();
    }
    if (m_lift)
    {
        m_lift->Shutdown();
    }
    if (m_holeSprite)
    {
        m_holeSprite->Shutdown();
    }
    if (m_liftButtonSprite)
    {
        m_liftButtonSprite->Shutdown();
    }

    for (auto& source : m_pulseSources)
    {
        source.Shutdown();
    }

    if (m_droneManager)
    {
        m_droneManager->Shutdown();
    }
}

// ---------------------------------------------------------------------------
// [DrawDebug]
// - 기능: 디버그 모드에서 낭떠러지 바운더리와 리프트 진입 장벽 라인을 색상 박스로 가시화합니다.
// ---------------------------------------------------------------------------
void Rooftop::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Visualize actual abyss/fall range regardless of unlock state.
    const float abyssWallHeight = 5000.0f;
    const float abyssWallWidth = 24.0f;
    const float abyssWallCenterY = MIN_Y + HEIGHT * 0.5f;
    const float abyssStartX = m_liftInitialX - (m_liftSize.x * 0.5f) + 90.0f;
    const float abyssEndX = m_liftTargetX + (m_liftSize.x * 0.5f) + 50.0f;
    debugRenderer.DrawBox(colorShader, { abyssStartX, abyssWallCenterY }, { abyssWallWidth, abyssWallHeight }, { 1.0f, 0.85f });
    debugRenderer.DrawBox(colorShader, { abyssEndX, abyssWallCenterY }, { abyssWallWidth, abyssWallHeight }, { 1.0f, 0.85f });

    if (m_isLiftGapUnlocked)
    {
        return;
    }

    // Visualize virtual X-walls that block crossing the lift gap before activation.
    const float wallHeight = 5000.0f;
    const float wallWidth = 30.0f;
    const float wallCenterY = MIN_Y + HEIGHT * 0.5f;
    const float wallLeftX = m_liftInitialX - (m_liftSize.x * 0.5f) + 90.0f;
    const float wallRightX = m_liftTargetX + (m_liftSize.x * 0.5f) + 50.0f;

    debugRenderer.DrawBox(colorShader, { wallLeftX, wallCenterY }, { wallWidth, wallHeight }, { 0.0f, 1.0f });
    debugRenderer.DrawBox(colorShader, { wallRightX, wallCenterY }, { wallWidth, wallHeight }, { 0.0f, 1.0f });
}

// ---------------------------------------------------------------------------
// [DrawSpriteOutlines]
// - 기능: 플레이어 접근 시 충전소, 엘리베이터 조작 버튼, 리프트 본체, 그리고 닫히지 않은 옥상 구멍 주위에 아웃라인 글로우를 렌더링합니다.
// ---------------------------------------------------------------------------
void Rooftop::DrawSpriteOutlines(Shader& outlineShader, Math::Vec2 playerPos, float proximityDist) const
{
    const float proxDistSq = proximityDist * proximityDist;

    for (const auto& source : m_pulseSources)
    {
        if (!source.HasSprite()) continue;
        float distSq = (playerPos - source.GetPosition()).LengthSq();
        if (distSq <= proxDistSq)
        {
            source.DrawOutline(outlineShader);
        }
    }

    if (!m_isClose && m_holeSprite)
    {
        // Hole sprite is wide, so use AABB proximity instead of center-distance.
        Math::Vec2 holeHalf = m_debugBoxSize * 0.5f;
        Math::Vec2 holeMin = m_debugBoxPos - holeHalf;
        Math::Vec2 holeMax = m_debugBoxPos + holeHalf;

        Math::Vec2 closestPointOnHole;
        closestPointOnHole.x = std::max(holeMin.x, std::min(playerPos.x, holeMax.x));
        closestPointOnHole.y = std::max(holeMin.y, std::min(playerPos.y, holeMax.y));

        float distSq = (playerPos - closestPointOnHole).LengthSq();
        if (distSq <= proxDistSq)
        {
            int w = m_holeSprite->GetWidth();
            int h = m_holeSprite->GetHeight();
            if (w > 0 && h > 0)
            {
                outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
                outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
                outlineShader.setFloat("outlineWidthTexels", 2.0f);

                Math::Matrix holeModel = Math::Matrix::CreateTranslation(m_debugBoxPos) * Math::Matrix::CreateScale(m_debugBoxSize);
                outlineShader.setMat4("model", holeModel);
                m_holeSprite->Draw(outlineShader, holeModel);
            }
        }
    }

    if (m_liftButtonSprite)
    {
        Math::Vec2 buttonHalf = m_liftButtonSize * 0.5f;
        Math::Vec2 buttonMin = m_liftButtonPos - buttonHalf;
        Math::Vec2 buttonMax = m_liftButtonPos + buttonHalf;

        Math::Vec2 closestPointOnButton;
        closestPointOnButton.x = std::max(buttonMin.x, std::min(playerPos.x, buttonMax.x));
        closestPointOnButton.y = std::max(buttonMin.y, std::min(playerPos.y, buttonMax.y));

        float distSq = (playerPos - closestPointOnButton).LengthSq();
        if (distSq <= proxDistSq)
        {
            int w = m_liftButtonSprite->GetWidth();
            int h = m_liftButtonSprite->GetHeight();
            if (w > 0 && h > 0)
            {
                outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
                outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
                outlineShader.setFloat("outlineWidthTexels", 2.0f);

                Math::Matrix buttonModel = Math::Matrix::CreateTranslation(m_liftButtonPos) * Math::Matrix::CreateScale(m_liftButtonSize);
                outlineShader.setMat4("model", buttonModel);
                m_liftButtonSprite->Draw(outlineShader, buttonModel);
            }
        }
    }

    // Lift platform sprite outline
    if (m_lift)
    {
        Math::Vec2 liftHalf = m_liftSize * 0.5f;
        Math::Vec2 liftMin = m_liftPos - liftHalf;
        Math::Vec2 liftMax = m_liftPos + liftHalf;

        Math::Vec2 closestPointOnLift;
        closestPointOnLift.x = std::max(liftMin.x, std::min(playerPos.x, liftMax.x));
        closestPointOnLift.y = std::max(liftMin.y, std::min(playerPos.y, liftMax.y));

        float distSq = (playerPos - closestPointOnLift).LengthSq();
        if (distSq <= proxDistSq)
        {
            int w = m_lift->GetWidth();
            int h = m_lift->GetHeight();
            if (w > 0 && h > 0)
            {
                outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
                outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
                outlineShader.setFloat("outlineWidthTexels", 2.0f);

                Math::Matrix liftModel = Math::Matrix::CreateTranslation(m_liftPos) * Math::Matrix::CreateScale(m_liftSize);
                outlineShader.setMat4("model", liftModel);
                m_lift->Draw(outlineShader, liftModel);
            }
        }
    }
}

const std::vector<Drone>& Rooftop::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Rooftop::GetDrones()
{
    return m_droneManager->GetDrones();
}

void Rooftop::ClearAllDrones()
{
    m_droneManager->ClearAllDrones();
}

// ---------------------------------------------------------------------------
// [GetLiftCountdownText]
// - 기능: 리프트 버튼 클릭 후 출발 직전까지 카운트다운 잔여 초 문구를 UI 표시용으로 포맷팅해 반환합니다.
// ---------------------------------------------------------------------------
std::string Rooftop::GetLiftCountdownText() const
{
    // Provide countdown timer text for UI display
    if (m_liftState == LiftState::Countdown)
    {
        int seconds = static_cast<int>(std::ceil(m_liftCountdown));
        return std::to_string(seconds) + " seconds before operation";
    }
    return "";
}