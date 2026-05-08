//Robot.cpp

#include "Robot.hpp"
#include "Player.hpp" 
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <random>
#include <cmath>
#include <algorithm>

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

constexpr float ATTACK_DASH_SPEED = 800.0f;

// Random number generation for robot behavior
static std::default_random_engine robot_gen;
static std::uniform_real_distribution<float> robot_dist(0.0f, 1.0f);

unsigned int Robot::LoadTexture(const char* path)
{
    unsigned int textureID;
    GL::GenTextures(1, &textureID);
    GL::BindTexture(GL_TEXTURE_2D, textureID);

    // Set texture wrapping/filtering options
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
        Logger::Instance().Log(Logger::Severity::Info, "Robot Texture Loaded: %s", path);
    }
    else
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to load texture: %s", path);
        stbi_image_free(data);
    }
    return textureID;
}

void Robot::Init(Math::Vec2 startPos)
{
    m_spawnPos = startPos;
    m_position = startPos;
    m_spawnX = startPos.x;
    m_velocity = { 0.0f, 0.0f };
    m_hp = 100.0f;
    m_maxHp = 100.0f;
    m_patrolSpeed = 140.0f;
    m_chaseSpeed = 350.0f;
    m_windupTime = 0.8f;
    m_state = RobotState::Patrol;
    m_directionX = 1.0f;

    // Load necessary assets
    m_textureID = LoadTexture("Asset/Robot.png");
    m_textureHighID = LoadTexture("Asset/Robot_High.png");
    m_textureLowID = LoadTexture("Asset/Robot_Low.png");

    m_soundHigh.Load("Asset/Robot_High.mp3", false);
    m_soundLow.Load("Asset/Robot_Low.mp3", false);

    float scaleFactor = 1.0f;
    m_size = { 396.0f * scaleFactor, 450.0f * scaleFactor };

    // Quad vertices: Position (x, y) and UV (u, v)
    float vertices[] = {
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,

        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &m_VAO);
    GL::GenBuffers(1, &m_VBO);
    GL::BindVertexArray(m_VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    // Texture Coordinate attribute
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);
}

void Robot::ApplyUndergroundDifficultyBoost()
{
    constexpr float kHpMul = 3.0f;
    constexpr float kSpeedMul = 1.5f;
    constexpr float kWindupReduction = 0.3f;

    m_maxHp *= kHpMul;
    m_hp *= kHpMul;
    m_patrolSpeed *= kSpeedMul;
    m_chaseSpeed *= kSpeedMul;
    m_windupTime = (std::max)(0.05f, m_windupTime - kWindupReduction);
}

void Robot::ApplyTrainEncounterDifficulty()
{
    constexpr float kHpMul = 3.0f;
    m_maxHp *= kHpMul;
    m_hp *= kHpMul;
    m_patrolSpeed = 92.f;
    m_chaseSpeed  = 168.f;
    m_windupTime  = (std::max)(0.05f, m_windupTime - 0.3f);
}

void Robot::ApplyTrainBerserkerProfile()
{
    m_chaseSpeed           = 252.f;
    m_patrolSpeed          = 120.f;
    m_trainBlindAggro      = true;
    m_trainBlindSweepTimer = 0.55f;
}

void Robot::Update(double dt, Player& player, const std::vector<ObstacleInfo>& obstacles, float mapMinX, float mapMaxX)
{
    if (m_state == RobotState::Dead) return;

    float fDt = static_cast<float>(dt);
    m_stateTimer -= fDt;
    m_attackCooldownTimer -= fDt;

    if (m_staggerCooldown > 0.0f)
    {
        m_staggerCooldown -= fDt;
    }

    m_trainDetectAlertTimer = std::max(0.f, m_trainDetectAlertTimer - fDt);

    Math::Vec2 playerPos = player.GetPosition();
    const bool trainNoDetect = player.IsTrainEnemyUndetectable() && m_trainCarSegment > 0;
    if (trainNoDetect
        && (m_state == RobotState::Chase || m_state == RobotState::Windup
            || m_state == RobotState::Attack || m_state == RobotState::Recover))
        m_state = RobotState::Patrol;

    float distToPlayer = std::abs(playerPos.x - m_position.x);
    float heightDiff = std::abs(playerPos.y - m_position.y);
    const float detHeight = (m_trainDeckPatrol || m_trainCarSegment > 0) ? 520.f : 300.f;

    // FSM State Logic
    switch (m_state)
    {
    case RobotState::Patrol:
        if (m_trainBlindAggro && !trainNoDetect)
        {
            m_directionX = (playerPos.x > m_position.x) ? 1.0f : -1.0f;
            m_velocity.x = m_directionX * m_chaseSpeed;

            m_trainBlindSweepTimer -= fDt;
            if (m_trainBlindSweepTimer <= 0.f && m_attackCooldownTimer <= 0.f)
            {
                m_trainBlindSweepTimer = 1.9f;
                DecideAttackPattern();
                m_state            = RobotState::Windup;
                m_stateTimer       = m_windupTime;
                m_velocity.x       = 0.f;
                m_hasDealtDamage   = false;
            }
            break;
        }
        // Reverse direction at patrol boundaries
        if (m_usePatrolWorldClamp)
        {
            if (m_position.x >= m_patrolWorldMaxX - 35.f)
                m_directionX = -1.0f;
            else if (m_position.x <= m_patrolWorldMinX + 35.f)
                m_directionX = 1.0f;
        }
        else if (m_position.x > m_spawnX + PATROL_RANGE)
        {
            m_directionX = -1.0f;
        }
        else if (m_position.x < m_spawnX - PATROL_RANGE)
        {
            m_directionX = 1.0f;
        }
        m_velocity.x = m_directionX * m_patrolSpeed;

        // Transition to Chase if player is detected
        if (!trainNoDetect && distToPlayer < DETECTION_RANGE && heightDiff < detHeight)
        {
            m_state                     = RobotState::Chase;
            m_trainDetectAlertTimer     = 0.95f;
            Logger::Instance().Log(Logger::Severity::Verbose, "Robot detected Player! Starting Chase.");
        }
        break;

    case RobotState::Chase:
        m_directionX = (playerPos.x > m_position.x) ? 1.0f : -1.0f;
        m_velocity.x = m_directionX * m_chaseSpeed;

        if (distToPlayer < ATTACK_RANGE)
        {
            if (m_attackCooldownTimer <= 0.0f)
            {
                DecideAttackPattern();
                m_state = RobotState::Windup;
                m_stateTimer = m_windupTime;
                m_velocity.x = 0.0f;
                m_hasDealtDamage = false;
            }
            else
            {
                m_velocity.x = 0.0f;
            }
        }
        else
        {
            if (m_trainBlindAggro && !trainNoDetect)
            {
                m_trainBlindSweepTimer -= fDt;
                if (m_trainBlindSweepTimer <= 0.f && m_attackCooldownTimer <= 0.f)
                {
                    m_trainBlindSweepTimer = 1.45f;
                    DecideAttackPattern();
                    m_state          = RobotState::Windup;
                    m_stateTimer     = m_windupTime;
                    m_velocity.x     = 0.f;
                    m_hasDealtDamage = false;
                }
            }
            else if (distToPlayer > DETECTION_RANGE)
            {
                m_state = RobotState::Patrol;
            }
        }
        break;

    case RobotState::Retreat:
        m_velocity.x = m_directionX * m_patrolSpeed;
        if (m_stateTimer <= 0.0f)
        {
            m_state = RobotState::Chase;
        }
        break;

    case RobotState::Windup:
        m_velocity.x = 0.0f;
        if (m_stateTimer <= 0.0f)
        {
            m_state = RobotState::Attack;
            m_stateTimer = ATTACK_DURATION;
            m_hasPlayedAttackSound = false;
            Logger::Instance().Log(Logger::Severity::Verbose, "Robot Attack! Type: %d", (int)m_currentAttack);
        }
        break;

    case RobotState::Attack:
        // Dash toward the player during the attack
        m_velocity.x = m_directionX * ATTACK_DASH_SPEED;

        if (!m_hasPlayedAttackSound)
        {
            if (m_currentAttack == AttackType::HighSweep)
            {
                m_soundHigh.Play();
            }
            else if (m_currentAttack == AttackType::LowSweep)
            {
                m_soundLow.Play();
            }
            m_hasPlayedAttackSound = true;
        }

        // Damage processing logic
        if (!m_hasDealtDamage)
        {
            Math::Vec2 attackBoxSize = { 590.0f, 150.0f };
            Math::Vec2 attackBoxPos = { 0.0f, 0.0f };

            // Adjust attack box height based on attack type
            if (m_currentAttack == AttackType::LowSweep)
            {
                attackBoxPos = { m_position.x, m_position.y - m_size.y / 2.0f + 75.0f };
            }
            else if (m_currentAttack == AttackType::HighSweep)
            {
                attackBoxPos = { m_position.x, m_position.y + m_size.y / 2.0f - 75.0f };
            }

            if (m_allowTrainCombatVsPlayer
                && Collision::CheckAABB(player.GetHitboxCenter(), player.GetHitboxSize(), attackBoxPos, attackBoxSize))
            {
                player.TakeDamage(20.0f);
                m_hasDealtDamage = true;
                Logger::Instance().Log(Logger::Severity::Verbose, "Player Hit by Robot (Inside Attack Box)!");
            }
        }

        if (m_stateTimer <= 0.0f)
        {
            m_state = RobotState::Recover;
            m_stateTimer = RECOVER_TIME;
        }
        break;

    case RobotState::Recover:
        m_velocity.x = 0.0f;
        if (m_stateTimer <= 0.0f)
        {
            m_state = RobotState::Chase;
            m_attackCooldownTimer = 1.0f;
        }
        break;

    case RobotState::Stagger:
        if (m_horzExternalImpulseTimer > 0.0f)
        {
            m_horzExternalImpulseTimer -= fDt;
            const float drag = std::pow(0.11f, fDt / 0.58f);
            m_velocity.x *= drag;
            if (std::abs(m_velocity.x) < 12.f)
                m_velocity.x = 0.0f;
        }
        else
        {
            m_velocity.x = 0.0f;
        }
        if (m_stateTimer <= 0.0f)
        {
            m_state = RobotState::Chase;
        }
        break;

    case RobotState::Dead:
        // Already handled by early return at the start of Update,
        // but required to silence compiler warnings about unhandled enum value.
        break;
    }

    // Ground-only movement (no gravity, vault jump, or climbing onto obstacles).
    m_velocity.y = 0.0f;

    // Map boundary constraints
    Math::Vec2 nextPos = m_position;
    nextPos.x += m_velocity.x * fDt;

    float halfW = m_size.x / 4.0f;
    if (nextPos.x - halfW < mapMinX) {
        nextPos.x = mapMinX + halfW;
        if (m_state == RobotState::Patrol || m_state == RobotState::Retreat) m_directionX = 1.0f;
    }
    if (nextPos.x + halfW > mapMaxX) {
        nextPos.x = mapMaxX - halfW;
        if (m_state == RobotState::Patrol || m_state == RobotState::Retreat) m_directionX = -1.0f;
    }

    // Obstacle collision detection (horizontal only)
    if (std::abs(m_velocity.x) > 0.1f)
    {
        for (const auto& obs : obstacles)
        {
            bool currentlyColliding = Collision::CheckAABB(m_position, m_size, obs.pos, obs.size);
            Math::Vec2 testPos = { nextPos.x, m_position.y };
            bool wouldCollide = Collision::CheckAABB(testPos, m_size, obs.pos, obs.size);

            if (!currentlyColliding && wouldCollide)
            {
                nextPos.x = m_position.x;
                if (m_state == RobotState::Patrol || m_state == RobotState::Retreat)
                    m_directionX = -m_directionX;
                break;
            }
        }
    }

    m_position.x = nextPos.x;
    m_position.y = m_groundLimitY + m_size.y / 2.0f;
    m_isOnGround = true;
}

void Robot::DecideAttackPattern()
{
    AttackType nextAttack = (robot_dist(robot_gen) > 0.5f) ? AttackType::LowSweep : AttackType::HighSweep;

    // Prevent more than 3 consecutive identical attacks
    if (nextAttack == m_lastAttack)
    {
        m_consecutiveAttackCount++;
        if (m_consecutiveAttackCount >= 3)
        {
            nextAttack = (m_lastAttack == AttackType::LowSweep) ? AttackType::HighSweep : AttackType::LowSweep;
            m_consecutiveAttackCount = 1;
        }
    }
    else
    {
        m_consecutiveAttackCount = 1;
    }

    m_currentAttack = nextAttack;
    m_lastAttack = nextAttack;
}

void Robot::Draw(const Shader& shader) const
{
    if (m_state == RobotState::Dead) return;

    shader.use();

    bool flipX = (m_directionX > 0.0f);

    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    shader.setBool("flipX", flipX);
    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);

    unsigned int textureToBind = m_textureID;

    // Use the base image for Windup state,
    // and bind the specific High/Low texture during the Attack state.
    if (m_state == RobotState::Attack)
    {
        if (m_currentAttack == AttackType::HighSweep)
        {
            textureToBind = m_textureHighID;
        }
        else if (m_currentAttack == AttackType::LowSweep)
        {
            textureToBind = m_textureLowID;
        }
    }

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureToBind);

    GL::BindVertexArray(m_VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Robot::DrawOutline(const Shader& outlineShader) const
{
    if (m_state == RobotState::Dead) return;

    outlineShader.use();

    bool flipX = (m_directionX > 0.0f);
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    outlineShader.setMat4("model", model);
    outlineShader.setBool("flipX", flipX);
    outlineShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
    outlineShader.setFloat("outlineWidthTexels", 2.0f);

    unsigned int textureToBind = m_textureID;
    if (m_state == RobotState::Attack)
    {
        if (m_currentAttack == AttackType::HighSweep) textureToBind = m_textureHighID;
        else if (m_currentAttack == AttackType::LowSweep) textureToBind = m_textureLowID;
    }

    // Robot textures are single-frame sprites.
    int texW = 396;
    int texH = 450;
    if (textureToBind == m_textureHighID || textureToBind == m_textureLowID)
    {
        texW = 590;
        texH = 450;
    }
    outlineShader.setVec2("texelSize", 1.0f / static_cast<float>(texW), 1.0f / static_cast<float>(texH));

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureToBind);
    GL::BindVertexArray(m_VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Robot::DrawGauge(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    (void)debugRenderer;
    // Same style behavior as Drone::DrawGauge: only when damaged.
    if (m_state == RobotState::Dead || m_hp >= m_maxHp) return;

    float barWidth = 150.0f;
    float barHeight = 15.0f;
    float yOffset = m_size.y / 2.0f + 30.0f;
    Math::Vec2 barCenterPos = { m_position.x, m_position.y + yOffset };

    Math::Vec2 bgSize = { barWidth + 4.0f, barHeight + 4.0f };
    Math::Matrix bgModel = Math::Matrix::CreateTranslation(barCenterPos) * Math::Matrix::CreateScale(bgSize);
    colorShader.setMat4("model", bgModel);
    colorShader.setVec3("objectColor", 0.0f, 0.0f, 0.0f);
    GL::BindVertexArray(m_VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    float ratio = m_hp / m_maxHp;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    float fillWidth = barWidth * ratio;
    float leftEdgeX = barCenterPos.x - (barWidth / 2.0f);
    float fillCenterX = leftEdgeX + (fillWidth / 2.0f);
    Math::Vec2 fgPos = { fillCenterX, barCenterPos.y };
    Math::Vec2 fgSize = { fillWidth, barHeight };
    Math::Matrix fgModel = Math::Matrix::CreateTranslation(fgPos) * Math::Matrix::CreateScale(fgSize);
    colorShader.setMat4("model", fgModel);

    float r = 1.0f;
    float g = 1.0f;
    if (ratio > 0.5f)
    {
        r = 2.0f * (1.0f - ratio); // green -> yellow
        g = 1.0f;
    }
    else
    {
        r = 1.0f;                  // yellow -> red
        g = 2.0f * ratio;
    }
    colorShader.setVec3("objectColor", r, g, 0.0f);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Robot::DrawAlert(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_state == RobotState::Dead) return;

    // Render the attack hitboxes visually during Windup and Attack states
    if (m_state == RobotState::Windup || m_state == RobotState::Attack)
    {
        if (m_currentAttack == AttackType::LowSweep)
        {
            float yPos = m_position.y - m_size.y / 2.0f + 75.0f;
            debugRenderer.DrawBox(colorShader, { m_position.x, yPos }, { 590.0f, 150.0f }, { 1.0f, 0.0f });
        }
        else if (m_currentAttack == AttackType::HighSweep)
        {
            float yPos = m_position.y + m_size.y / 2.0f - 75.0f;
            debugRenderer.DrawBox(colorShader, { m_position.x, yPos }, { 590.0f, 150.0f }, { 1.0f, 0.0f });
        }
    }
}

void Robot::DrawTrainDetectAlert(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_state == RobotState::Dead || m_trainDetectAlertTimer <= 0.f)
        return;

    const float bob  = std::sin(m_trainDetectAlertTimer * 14.f) * 6.f;
    const float topY = m_position.y + m_size.y * 0.5f + 95.f + bob;
    const Math::Vec2 markCenter{ m_position.x, topY };

    debugRenderer.DrawBox(colorShader, markCenter, { 40.f, 46.f }, 0.1f, 0.1f, 0.1f);
    debugRenderer.DrawBox(colorShader, { markCenter.x, markCenter.y + 10.f }, { 9.f, 26.f }, 1.0f, 0.85f, 0.05f);
    debugRenderer.DrawBox(colorShader, { markCenter.x, markCenter.y - 12.f }, { 11.f, 11.f }, 1.0f, 0.85f, 0.05f);
}

void Robot::TakeDamage(float amount, bool applyStagger)
{
    if (m_state == RobotState::Dead) return;

    m_hp -= amount;

    if (applyStagger && m_staggerCooldown <= 0.0f)
    {
        m_state = RobotState::Stagger;
        m_stateTimer = 0.3f;
        m_staggerCooldown = STAGGER_COOLDOWN_DURATION;
        Logger::Instance().Log(Logger::Severity::Verbose, "Robot Staggered!");
    }

    if (m_hp <= 0.0f)
    {
        m_hp = 0.0f;
        m_state = RobotState::Dead;
        Logger::Instance().Log(Logger::Severity::Verbose, "Sweep Stalker Destroyed!");
    }
}

void Robot::ApplyPulseImpact(Math::Vec2 impulse, float damage)
{
    if (m_state == RobotState::Dead)
        return;
    m_velocity.x += impulse.x;
    m_velocity.y = 0.f;
    m_horzExternalImpulseTimer = 0.58f;
    m_hp -= damage;
    if (m_hp <= 0.0f)
    {
        m_hp                = 0.0f;
        m_state             = RobotState::Dead;
        m_horzExternalImpulseTimer = 0.f;
        Logger::Instance().Log(Logger::Severity::Verbose, "Sweep Stalker Destroyed!");
        return;
    }
    m_state             = RobotState::Stagger;
    m_stateTimer        = 0.55f;
    m_staggerCooldown   = STAGGER_COOLDOWN_DURATION;
}

void Robot::Reset()
{
    const int   trainSeg    = m_trainCarSegment;
    const bool  deckPatrol  = m_trainDeckPatrol;
    const float trainGround = m_groundLimitY;
    const bool  blindKeep   = m_trainBlindAggro;

    m_groundLimitY          = -1910.0f;
    m_position              = m_spawnPos;
    m_velocity              = { 0.0f, 0.0f };
    m_hp                    = m_maxHp;  // preserves difficulty boost
    m_state                 = RobotState::Patrol;
    m_directionX            = 1.0f;
    m_stateTimer            = 0.0f;
    m_currentAttack         = AttackType::None;
    m_lastAttack            = AttackType::None;
    m_consecutiveAttackCount = 0;
    m_attackCooldownTimer   = 0.0f;
    m_staggerCooldown       = 0.0f;
    m_hasDealtDamage        = false;
    m_horzExternalImpulseTimer = 0.f;
    m_isOnGround            = false;
    m_hasPlayedAttackSound  = false;
    m_trainCarSegment          = 0;
    m_trainDeckPatrol          = false;
    m_usePatrolWorldClamp      = false;
    m_allowTrainCombatVsPlayer = true;
    m_trainDetectAlertTimer    = 0.f;
    m_trainBlindAggro          = false;
    m_trainBlindSweepTimer     = 0.f;

    m_groundLimitY = trainGround;

    if (trainSeg > 0)
    {
        m_trainCarSegment         = trainSeg;
        m_trainDeckPatrol         = deckPatrol;
        m_allowTrainCombatVsPlayer = true;
        m_trainBlindAggro          = blindKeep;
        if (blindKeep)
            m_trainBlindSweepTimer = 0.55f;
    }
}

void Robot::Shutdown()
{
    // Cleanup OpenGL resources
    GL::DeleteVertexArrays(1, &m_VAO);
    GL::DeleteBuffers(1, &m_VBO);
    GL::DeleteTextures(1, &m_textureID);
    GL::DeleteTextures(1, &m_textureHighID);
    GL::DeleteTextures(1, &m_textureLowID);
    
    m_soundHigh.Stop();
    m_soundLow.Stop();
}