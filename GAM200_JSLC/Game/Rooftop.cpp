//Rooftop.cpp

#include "Rooftop.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "../Game/PulseCore.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm>
#include <cmath>

void Rooftop::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Rooftop.png");
    m_closeBackground = std::make_unique<Background>();
    m_closeBackground->Initialize("Asset/Rooftop_Close.png");

    m_lift = std::make_unique<Background>();
    m_lift->Initialize("Asset/Lift.png");

    float liftWidth = 351.f;
    float liftHeight = 345.f;
    float liftTopLeftX = 13745.f;
    float liftTopY = 1799.f;

    m_liftPos = { liftTopLeftX + (liftWidth / 2.0f), liftTopY - (liftHeight / 2.0f) };
    m_liftSize = { liftWidth, liftHeight };
    m_liftInitialX = m_liftPos.x;

    float targetLeftEdgeX = 14928.0f;
    m_liftTargetX = targetLeftEdgeX + (liftWidth / 2.0f);

    m_liftState = LiftState::Idle;
    m_isPlayerNearLift = false;
    m_liftSpeed = 250.0f;
    m_liftCountdown = 3.0f;
    m_isLiftActivated = false;
    m_liftDirection = 1.0f;

    m_size = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH / 2.0f, MIN_Y + HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();

    m_droneManager->SpawnDrone({ 8700.0f, 1700.0f }, "Asset/drone.png", false);
    m_droneManager->SpawnDrone({ 13500.0f, 1900.0f }, "Asset/drone.png", true);
    m_droneManager->SpawnDrone({ 14500.0f, 1750.0f }, "Asset/drone.png", true);
    m_droneManager->SpawnDrone({ 15500.0f, 1700.0f }, "Asset/drone.png", false);
    m_droneManager->SpawnDrone({ 17250.0f, 1800.0f }, "Asset/drone.png", true);

    float width = 785.f;
    float height = 172.f;
    float topLeftX = 9951.f;
    float game_Y_top = 1506.f;

    m_debugBoxPos = { topLeftX + (width / 2.0f), game_Y_top - (height / 2.0f) };
    m_debugBoxSize = { width, height };

    float pulseWidth1 = 333.f;
    float pulseHeight1 = 240.f;
    float pulseTopLeftX1 = 12521.f;
    float pulseTopLeftY1 = 1700.f;
    Math::Vec2 pulseCenter1 = { pulseTopLeftX1 + (pulseWidth1 / 2.0f), pulseTopLeftY1 - (pulseHeight1 / 2.0f) };
    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(pulseCenter1, { pulseWidth1, pulseHeight1 }, 100.f);

    float pulseWidth2 = 333.f;
    float pulseHeight2 = 240.f;
    float pulseTopLeftX2 = 12900.f;
    float pulseTopLeftY2 = 1700.f;
    Math::Vec2 pulseCenter2 = { pulseTopLeftX2 + (pulseWidth2 / 2.0f), pulseTopLeftY2 - (pulseHeight2 / 2.0f) };
    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(pulseCenter2, { pulseWidth2, pulseHeight2 }, 100.f);

    m_isClose = false;
    m_isPlayerClose = false;
}

void Rooftop::Update(double dt, Player& player, Math::Vec2 playerHitboxSize, Input::Input& input)
{
    float oldLiftX = m_liftPos.x;
    Math::Vec2 playerPos = player.GetPosition();

    bool isPlayerInRooftop = (playerPos.y >= MIN_Y - 1000.0f && playerPos.y <= MIN_Y + HEIGHT &&
        playerPos.x >= MIN_X && playerPos.x <= MIN_X + WIDTH);

    if (!isPlayerInRooftop)
    {
        return;
    }

    Math::Vec2 liftHalfSizeCheck = m_liftSize / 2.0f;
    Math::Vec2 liftMinCheck = m_liftPos - liftHalfSizeCheck;
    Math::Vec2 liftMaxCheck = m_liftPos + liftHalfSizeCheck;

    Math::Vec2 playerHalfCheck = playerHitboxSize / 2.0f;
    Math::Vec2 playerMinCheck = playerPos - playerHalfCheck;
    Math::Vec2 playerMaxCheck = playerPos + playerHalfCheck;

    bool isTouchingLift = (playerMaxCheck.x > liftMinCheck.x && playerMinCheck.x < liftMaxCheck.x &&
        playerMaxCheck.y > liftMinCheck.y && playerMinCheck.y < liftMaxCheck.y);

    float safeGroundY = MIN_Y + 180.0f + 200.0f;
    float abyssGroundY = MIN_Y - 800.0f;

    float abyssStartX = m_liftInitialX - (m_liftSize.x / 2.0f) + 20.0f;
    float abyssEndX = m_liftTargetX + (m_liftSize.x / 2.0f) - 20.0f;

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

    {
        Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        Math::Vec2 boxHalfSize = m_debugBoxSize / 2.0f;
        Math::Vec2 boxMin = m_debugBoxPos - boxHalfSize;
        Math::Vec2 boxMax = m_debugBoxPos + boxHalfSize;
        Math::Vec2 closestPointOnBox;
        closestPointOnBox.x = std::clamp(playerPos.x, boxMin.x, boxMax.x);
        closestPointOnBox.y = std::clamp(playerPos.y, boxMin.y, boxMax.y);

        Math::Vec2 closestPointOnPlayer;
        closestPointOnPlayer.x = std::clamp(closestPointOnBox.x, playerMin.x, playerMax.x);
        closestPointOnPlayer.y = std::clamp(closestPointOnBox.y, playerMin.y, playerMax.y);

        float distanceSq = (closestPointOnPlayer - closestPointOnBox).LengthSq();
        const float PROXIMITY_RANGE_SQ = 100.0f * 100.0f;

        m_isPlayerClose = (distanceSq <= PROXIMITY_RANGE_SQ);

        if (m_isPlayerClose && input.IsKeyTriggered(Input::Key::F) && !m_isClose)
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

    {
        Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
        Math::Vec2 liftHalfSize = m_liftSize / 2.0f;

        Math::Vec2 liftMin = m_liftPos - liftHalfSize;
        Math::Vec2 liftMax = m_liftPos + liftHalfSize;

        Math::Vec2 closestPointOnLift;
        closestPointOnLift.x = std::clamp(playerPos.x, liftMin.x, liftMax.x);
        closestPointOnLift.y = std::clamp(playerPos.y, liftMin.y, liftMax.y);

        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        Math::Vec2 closestPointOnPlayer;
        closestPointOnPlayer.x = std::clamp(closestPointOnLift.x, playerMin.x, playerMax.x);
        closestPointOnPlayer.y = std::clamp(closestPointOnLift.y, playerMin.y, playerMax.y);

        float distanceSq = (closestPointOnPlayer - closestPointOnLift).LengthSq();
        const float LIFT_PROXIMITY_RANGE_SQ = 150.0f * 150.0f;

        m_isPlayerNearLift = (distanceSq <= LIFT_PROXIMITY_RANGE_SQ) && (m_liftState == LiftState::Idle);
    }

    if (m_liftState == LiftState::Idle)
    {
        if (m_isPlayerNearLift && input.IsKeyTriggered(Input::Key::F))
        {
            const float LIFT_COST = 8.0f;
            Pulse& pulse = player.GetPulseCore().getPulse();

            if (pulse.Value() >= LIFT_COST)
            {
                pulse.spend(LIFT_COST);
                m_liftState = LiftState::Countdown;
                m_liftCountdown = 3.0f;
                m_isLiftActivated = true;

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

        if (m_liftDirection > 0.0f)
        {
            if (m_liftPos.x >= m_liftTargetX)
            {
                m_liftPos.x = m_liftTargetX;
                m_liftState = LiftState::AtDestination;
            }
        }
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

    float leftBoundary = MIN_X;
    if (currentPlayerPos.x - playerHalfSize.x < leftBoundary)
    {
        player.SetPosition({ leftBoundary + playerHalfSize.x, currentPlayerPos.y });
    }

    float rightBoundary = MIN_X + WIDTH;
    if (currentPlayerPos.x + playerHalfSize.x > rightBoundary)
    {
        player.SetPosition({ rightBoundary - playerHalfSize.x, currentPlayerPos.y });
    }

    if (!m_isClose)
    {
        Math::Vec2 holeHalfSize = m_debugBoxSize * 0.5f;
        Math::Vec2 holeMin = m_debugBoxPos - holeHalfSize;
        Math::Vec2 holeMax = m_debugBoxPos + holeHalfSize;

        Math::Vec2 playerMin = currentPlayerPos - playerHalfSize;
        Math::Vec2 playerMax = currentPlayerPos + playerHalfSize;

        if (playerMax.x > holeMin.x && playerMin.x < holeMax.x &&
            playerMax.y > holeMin.y && playerMin.y < holeMax.y)
        {
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

    if (currentPlayerPos.x > abyssStartX && currentPlayerPos.x < abyssEndX)
    {
        if (!isTouchingLift && currentPlayerPos.y < 1500.0f)
        {
            const float DEATH_DRAIN_RATE = 100.0f;
            player.GetPulseCore().getPulse().spend(DEATH_DRAIN_RATE * static_cast<float>(dt));

            Logger::Instance().Log(Logger::Severity::Event, "Falling into abyss! Pulse draining...");
        }
    }

    m_droneManager->Update(dt, player, playerHitboxSize, false);
}

void Rooftop::Draw(Shader& shader) const
{
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

    Math::Matrix liftModel = Math::Matrix::CreateTranslation(m_liftPos) * Math::Matrix::CreateScale(m_liftSize);
    shader.setMat4("model", liftModel);
    m_lift->Draw(shader, liftModel);

    m_droneManager->Draw(shader);
}

void Rooftop::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Rooftop::Shutdown()
{
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
    for (const auto& source : m_pulseSources)
    {
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 1.0f, 0.5f });
    }
    Math::Vec2 debugColor = m_isPlayerClose ? Math::Vec2(0.0f, 1.0f) : Math::Vec2(1.0f, 0.0f);
    debugRenderer.DrawBox(colorShader, m_debugBoxPos, m_debugBoxSize, debugColor);

    Math::Vec2 liftDebugColor = m_isPlayerNearLift ? Math::Vec2(0.0f, 1.0f) : Math::Vec2(1.0f, 0.0f);
    debugRenderer.DrawBox(colorShader, m_liftPos, m_liftSize, liftDebugColor);

    float leftBoundary = MIN_X;
    float rightBoundary = MIN_X + WIDTH;
    float boundaryHeight = HEIGHT;
    Math::Vec2 leftBoundaryPos = { leftBoundary, MIN_Y + boundaryHeight / 2.0f };
    Math::Vec2 rightBoundaryPos = { rightBoundary, MIN_Y + boundaryHeight / 2.0f };
    Math::Vec2 boundarySize = { 10.0f, boundaryHeight };

    debugRenderer.DrawBox(colorShader, leftBoundaryPos, boundarySize, { 1.0f, 0.0f });
    debugRenderer.DrawBox(colorShader, rightBoundaryPos, boundarySize, { 1.0f, 0.0f });
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
    if (m_liftState == LiftState::Countdown)
    {
        int seconds = static_cast<int>(std::ceil(m_liftCountdown));
        return std::to_string(seconds) + " seconds before operation";
    }
    return "";
}