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

void Rooftop::Initialize()
{
    // Initialize background assets for different states
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Rooftop.png");
    m_closeBackground = std::make_unique<Background>();
    m_closeBackground->Initialize("Asset/Rooftop_Close.png");

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

void Rooftop::Update(double dt, Player& player, Math::Vec2 playerHitboxSize, Input::Input& input,
                     Math::Vec2 mouseWorldPos, bool isLeftClickTriggered)
{
    float oldLiftX = m_liftPos.x;
    Math::Vec2 playerPos = player.GetPosition();
    auto clampf = [](float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); };

    // Check if the player is within the rooftop level boundaries
    bool isPlayerInRooftop = (playerPos.y >= MIN_Y - 1000.0f && playerPos.y <= MIN_Y + HEIGHT &&
        playerPos.x >= MIN_X && playerPos.x <= MIN_X + WIDTH);

    if (!isPlayerInRooftop)
    {
        return;
    }

    // Calculate lift and player AABB boundaries for collision checking
    Math::Vec2 liftHalfSizeCheck = m_liftSize / 2.0f;
    Math::Vec2 liftMinCheck = m_liftPos - liftHalfSizeCheck;
    Math::Vec2 liftMaxCheck = m_liftPos + liftHalfSizeCheck;

    Math::Vec2 playerHalfCheck = playerHitboxSize / 2.0f;
    Math::Vec2 playerMinCheck = playerPos - playerHalfCheck;
    Math::Vec2 playerMaxCheck = playerPos + playerHalfCheck;

    // Determine if the player is currently touching/standing on the lift
    bool isTouchingLift = (playerMaxCheck.x > liftMinCheck.x && playerMinCheck.x < liftMaxCheck.x &&
        playerMaxCheck.y > liftMinCheck.y && playerMinCheck.y < liftMaxCheck.y);

    // Define Y levels for safe ground vs the falling abyss
    float safeGroundY = MIN_Y + 180.0f + 200.0f;
    float abyssGroundY = MIN_Y - 800.0f;

    // Define X range of the abyss gap
    float abyssStartX = m_liftInitialX - (m_liftSize.x / 2.0f) + 20.0f;
    float abyssEndX = m_liftTargetX + (m_liftSize.x / 2.0f) - 20.0f;

    // Manage ground level logic: if in gap, player must be on lift to avoid falling
    if (playerPos.x > abyssStartX && playerPos.x < abyssEndX)
    {
        if (isTouchingLift)
        {
            player.SetCurrentGroundLevel(safeGroundY);
        }
        else
        {
            player.SetCurrentGroundLevel(abyssGroundY);

            if (playerPos.y > abyssGroundY + 50.0f)
            {
                player.SetOnGround(false);
            }
        }
    }
    else
    {
        player.SetCurrentGroundLevel(safeGroundY);
    }

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
        if (m_isPlayerClose && input.IsMouseButtonTriggered(Input::MouseButton::Left) && !m_isClose)
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
                m_isLiftActivated = true;

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
    if (m_isLiftActivated)
    {
        Math::Vec2 liftHalfSize = m_liftSize * 0.5f;
        Math::Vec2 liftMin = m_liftPos - liftHalfSize;
        Math::Vec2 liftMax = m_liftPos + liftHalfSize;

        Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;
        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        if (playerMax.x > liftMin.x && playerMin.x < liftMax.x &&
            playerMax.y > liftMin.y && playerMin.y < liftMax.y)
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

    // AABB Collision with the rooftop hole (only if it is not yet closed)
    if (!m_isClose && !blockedByHoleWall)
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

    // AABB Collision with the lift platform (acts as a solid block when not activated/moving)
    if (!m_isLiftActivated && m_liftState != LiftState::AtDestination)
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
                player.SetPosition({ currentPlayerPos.x, liftMax.y + playerHalfSize.y });
            }
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
    m_droneManager->Update(dt, player, playerHitboxSize, false);
}

void Rooftop::Draw(Shader& shader) const
{
    // Render current background (dark or closed-hole version)
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);

    if (m_isClose)
    {
        m_closeBackground->Draw(shader, model);
    }
    else
    {
        m_background->Draw(shader, model);
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

    // Render drones
    m_droneManager->Draw(shader);
}

void Rooftop::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Render enemy detection radars
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Rooftop::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Render enemy health/status gauges
    m_droneManager->DrawGauges(colorShader, debugRenderer);
}

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

void Rooftop::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // No rectangle debug boxes in Rooftop on Tab debug mode.
    (void)colorShader;
    (void)debugRenderer;
}

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