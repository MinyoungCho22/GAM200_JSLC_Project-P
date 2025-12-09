#include "Robot.hpp"
#include "Player.hpp" 
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <iostream>
#include <random>
#include <cmath>

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

static std::default_random_engine robot_gen;
static std::uniform_real_distribution<float> robot_dist(0.0f, 1.0f);

void Robot::Init(Math::Vec2 startPos)
{
    m_position = startPos;
    m_velocity = { 0.0f, 0.0f };
    m_hp = 100.0f;
    m_state = RobotState::Patrol;

    const char* texturePath = "Asset/Robot.png";

    GL::GenTextures(1, &m_textureID);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);

        float scaleFactor = 1.0f;
        m_size = { 396.0f * scaleFactor, 450.0f * scaleFactor };

        stbi_image_free(data);
        Logger::Instance().Log(Logger::Severity::Info, "Robot initialized at (%.1f, %.1f)", startPos.x, startPos.y);
    }
    else
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to load Robot texture: %s", texturePath);
    }

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

    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);
}

void Robot::Update(double dt, Player& player, const std::vector<ObstacleInfo>& obstacles, float mapMinX, float mapMaxX)
{
    if (m_state == RobotState::Dead) return;

    float fDt = static_cast<float>(dt);
    m_stateTimer -= fDt;
    m_attackCooldownTimer -= fDt;

    Math::Vec2 playerPos = player.GetPosition();
    float distToPlayer = std::abs(playerPos.x - m_position.x);
    float heightDiff = std::abs(playerPos.y - m_position.y);

    switch (m_state)
    {
    case RobotState::Patrol:
        if (m_stateTimer <= 0.0f)
        {
            m_directionX = (robot_dist(robot_gen) > 0.5f) ? 1.0f : -1.0f;
            m_stateTimer = 2.0f + robot_dist(robot_gen) * 2.0f;
        }
        m_velocity.x = m_directionX * PATROL_SPEED;

        if (distToPlayer < DETECTION_RANGE && heightDiff < 300.0f)
        {
            m_state = RobotState::Chase;
            Logger::Instance().Log(Logger::Severity::Event, "Robot detected Player! Starting Chase.");
        }
        break;

    case RobotState::Chase:
        m_directionX = (playerPos.x > m_position.x) ? 1.0f : -1.0f;
        m_velocity.x = m_directionX * CHASE_SPEED;

        if (distToPlayer < ATTACK_RANGE)
        {
            if (m_attackCooldownTimer <= 0.0f)
            {
                DecideAttackPattern();
                m_state = RobotState::Windup;
                m_stateTimer = WINDUP_TIME;
                m_velocity.x = 0.0f;

                m_hasDealtDamage = false;
            }
            else
            {
                m_velocity.x = 0.0f;
            }
        }
        else if (distToPlayer > DETECTION_RANGE * 1.5f)
        {
            m_state = RobotState::Patrol;
        }
        break;

    case RobotState::Retreat:
        m_velocity.x = m_directionX * PATROL_SPEED;

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
            Logger::Instance().Log(Logger::Severity::Event, "Robot Attack! Type: %d", (int)m_currentAttack);
        }
        break;

    case RobotState::Attack:
        m_velocity.x = 0.0f;

        // 빨간 박스(경고 박스)와 플레이어 히트박스 충돌 검사
        if (!m_hasDealtDamage)
        {
            // DrawAlert에서 그리는 빨간 박스와 동일한 크기 및 위치
            Math::Vec2 attackBoxSize = { 600.0f, 50.0f }; // 너비 600, 높이 50
            Math::Vec2 attackBoxPos = { 0.0f, 0.0f };

            if (m_currentAttack == AttackType::LowSweep)
            {
                // DrawAlert의 LowSweep 박스 위치 계산식
                attackBoxPos = { m_position.x, m_position.y - m_size.y / 2.0f + 50.0f };
            }
            else if (m_currentAttack == AttackType::HighSweep)
            {
                // DrawAlert의 HighSweep 박스 위치 계산식
                attackBoxPos = { m_position.x, m_position.y + m_size.y / 2.0f - 50.0f };
            }

            // 플레이어가 실제 빨간 박스 안에 들어와 있는지 체크 (AABB 충돌)
            if (Collision::CheckAABB(player.GetHitboxCenter(), player.GetHitboxSize(), attackBoxPos, attackBoxSize))
            {
                player.TakeDamage(20.0f);
                m_hasDealtDamage = true;
                Logger::Instance().Log(Logger::Severity::Event, "Player Hit by Robot (Inside Attack Box)!");
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
        m_velocity.x = 0.0f;
        if (m_stateTimer <= 0.0f)
        {
            m_state = RobotState::Chase;
        }
        break;
    }

    m_velocity.y += GRAVITY * fDt;

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

    for (const auto& obs : obstacles)
    {
        if (Collision::CheckAABB(nextPos, { halfW * 2, m_size.y }, obs.pos, obs.size))
        {
            if (m_velocity.x > 0) nextPos.x = obs.pos.x - obs.size.x / 2.0f - halfW;
            else if (m_velocity.x < 0) nextPos.x = obs.pos.x + obs.size.x / 2.0f + halfW;

            if (m_state == RobotState::Patrol)
            {
                m_directionX *= -1.0f;
            }
            else if (m_state == RobotState::Chase)
            {
                m_state = RobotState::Retreat;
                m_directionX *= -1.0f;
                m_stateTimer = 1.5f;
                Logger::Instance().Log(Logger::Severity::Event, "Robot hit wall, Retreating!");
            }
            else if (m_state == RobotState::Retreat)
            {
                m_directionX *= -1.0f;
            }

            break;
        }
    }
    m_position.x = nextPos.x;

    m_isOnGround = false;
    nextPos = m_position;
    nextPos.y += m_velocity.y * fDt;

    for (const auto& obs : obstacles)
    {
        if (Collision::CheckAABB(nextPos, { halfW * 2, m_size.y }, obs.pos, obs.size))
        {
            if (m_velocity.y < 0 && m_position.y > obs.pos.y)
            {
                nextPos.y = obs.pos.y + obs.size.y / 2.0f + m_size.y / 2.0f;
                m_velocity.y = 0.0f;
                m_isOnGround = true;
            }
        }
    }

    float groundLimit = -1860.0f;
    if (nextPos.y - m_size.y / 2.0f < groundLimit)
    {
        nextPos.y = groundLimit + m_size.y / 2.0f;
        m_velocity.y = 0.0f;
        m_isOnGround = true;
    }

    m_position.y = nextPos.y;
}

void Robot::DecideAttackPattern()
{
    AttackType nextAttack = (robot_dist(robot_gen) > 0.5f) ? AttackType::LowSweep : AttackType::HighSweep;

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

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);
    GL::BindVertexArray(m_VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Robot::DrawGauge(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_state == RobotState::Dead) return;

    float barWidth = 150.0f;
    float barHeight = 15.0f;
    float yOffset = m_size.y / 2.0f + 30.0f;

    Math::Vec2 barPos = { m_position.x, m_position.y + yOffset };

    debugRenderer.DrawBox(colorShader, barPos, { barWidth + 4.0f, barHeight + 4.0f }, { 0.0f, 0.0f });

    float ratio = m_hp / m_maxHp;
    if (ratio < 0.0f) ratio = 0.0f;
    float fillWidth = barWidth * ratio;

    float leftEdge = barPos.x - (barWidth / 2.0f);
    float fillCenterX = leftEdge + (fillWidth / 2.0f);

    debugRenderer.DrawBox(colorShader, { fillCenterX, barPos.y }, { fillWidth, barHeight }, { 1.0f, 0.0f });
}

void Robot::DrawAlert(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_state == RobotState::Dead) return;

    if (m_state == RobotState::Windup || m_state == RobotState::Attack)
    {
        if (m_currentAttack == AttackType::LowSweep)
        {
            float yPos = m_position.y - m_size.y / 2.0f + 50.0f;
            debugRenderer.DrawBox(colorShader, { m_position.x, yPos }, { 600.0f, 50.0f }, { 1.0f, 0.0f });
        }
        else if (m_currentAttack == AttackType::HighSweep)
        {
            float yPos = m_position.y + m_size.y / 2.0f - 50.0f;
            debugRenderer.DrawBox(colorShader, { m_position.x, yPos }, { 600.0f, 50.0f }, { 1.0f, 0.0f });
        }
    }
}

void Robot::TakeDamage(float amount)
{
    if (m_state == RobotState::Dead) return;

    m_hp -= amount;

    m_state = RobotState::Stagger;
    m_stateTimer = 0.3f;

    if (m_hp <= 0.0f)
    {
        m_hp = 0.0f;
        m_state = RobotState::Dead;
        Logger::Instance().Log(Logger::Severity::Event, "Sweep Stalker Destroyed!");
    }
}

void Robot::Shutdown()
{
    GL::DeleteVertexArrays(1, &m_VAO);
    GL::DeleteBuffers(1, &m_VBO);
    GL::DeleteTextures(1, &m_textureID);
}