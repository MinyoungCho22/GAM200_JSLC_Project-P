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

// Robot attack dash speed
constexpr float ATTACK_DASH_SPEED = 800.0f;

static std::default_random_engine robot_gen;
static std::uniform_real_distribution<float> robot_dist(0.0f, 1.0f);

unsigned int Robot::LoadTexture(const char* path)
{
    unsigned int textureID;
    GL::GenTextures(1, &textureID);
    GL::BindTexture(GL_TEXTURE_2D, textureID);

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
    m_position = startPos;
    m_spawnX = startPos.x;
    m_velocity = { 0.0f, 0.0f };
    m_hp = 100.0f;
    m_state = RobotState::Patrol;
    m_directionX = 1.0f;

    m_textureID = LoadTexture("Asset/Robot.png");
    m_textureHighID = LoadTexture("Asset/Robot_High.png");
    m_textureLowID = LoadTexture("Asset/Robot_Low.png");

    m_soundHigh.Load("Asset/Robot_High.mp3", false);
    m_soundLow.Load("Asset/Robot_Low.mp3", false);

    float scaleFactor = 1.0f;
    m_size = { 396.0f * scaleFactor, 450.0f * scaleFactor };

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

    if (m_staggerCooldown > 0.0f)
    {
        m_staggerCooldown -= fDt;
    }

    Math::Vec2 playerPos = player.GetPosition();
    float distToPlayer = std::abs(playerPos.x - m_position.x);
    float heightDiff = std::abs(playerPos.y - m_position.y);

    switch (m_state)
    {
    case RobotState::Patrol:
        if (m_position.x > m_spawnX + PATROL_RANGE)
        {
            m_directionX = -1.0f;
        }
        else if (m_position.x < m_spawnX - PATROL_RANGE)
        {
            m_directionX = 1.0f;
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
        else if (distToPlayer > DETECTION_RANGE)
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
            m_hasPlayedAttackSound = false;
            Logger::Instance().Log(Logger::Severity::Event, "Robot Attack! Type: %d", (int)m_currentAttack);
        }
        break;

    case RobotState::Attack:
        // ���� ����: �÷��̾� ������ ������ ���
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

        if (!m_hasDealtDamage)
        {
            Math::Vec2 attackBoxSize = { 590.0f, 150.0f };
            Math::Vec2 attackBoxPos = { 0.0f, 0.0f };

            if (m_currentAttack == AttackType::LowSweep)
            {
                attackBoxPos = { m_position.x, m_position.y - m_size.y / 2.0f + 75.0f };
            }
            else if (m_currentAttack == AttackType::HighSweep)
            {
                attackBoxPos = { m_position.x, m_position.y + m_size.y / 2.0f - 75.0f };
            }

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

    m_position.x = nextPos.x;

    m_isOnGround = false;
    nextPos = m_position;
    nextPos.y += m_velocity.y * fDt;

    float groundLimit = -1910.0f;
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

    unsigned int textureToBind = m_textureID;

    // [����] Windup(�غ�) ������ ���� �⺻ �̹����� �����ϰ�,
    // ���� Attack(����) ������ ���� ���/�ϴ� ���� �̹����� ���ε��մϴ�.
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

void Robot::TakeDamage(float amount)
{
    if (m_state == RobotState::Dead) return;

    m_hp -= amount;

    if (m_staggerCooldown <= 0.0f)
    {
        m_state = RobotState::Stagger;
        m_stateTimer = 0.3f;
        m_staggerCooldown = STAGGER_COOLDOWN_DURATION;
        Logger::Instance().Log(Logger::Severity::Event, "Robot Staggered!");
    }

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
    GL::DeleteTextures(1, &m_textureHighID);
    GL::DeleteTextures(1, &m_textureLowID);
    m_soundHigh.Stop();
    m_soundLow.Stop();
}
