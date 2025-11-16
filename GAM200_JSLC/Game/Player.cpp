//Player.cpp
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>

#pragma warning(push, 0)
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "../Engine/Logger.hpp"
#pragma warning(pop)

const float GRAVITY = -1500.0f;
const float GROUND_LEVEL = 180.0f;

void AnimationData::Update(float dt)
{
    timer += dt;
    if (timer >= frameDuration)
    {
        timer -= frameDuration;
        currentFrame = (currentFrame + 1) % totalFrames;
    }
}

void AnimationData::Reset()
{
    currentFrame = 0;
    timer = 0.0f;
}

bool Player::LoadAnimation(AnimationState state, const char* texturePath, int totalFrames, float frameDuration)
{
    int index = static_cast<int>(state);
    AnimationData& anim = m_animations[index];

    GL::GenTextures(1, &anim.textureID);
    GL::BindTexture(GL_TEXTURE_2D, anim.textureID);

    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (data)
    {
        anim.texWidth = width;
        anim.texHeight = height;
        anim.frameWidth = width / totalFrames;
        anim.totalFrames = totalFrames;
        anim.frameDuration = frameDuration;

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);
        return true;
    }
    else
    {
        std::cout << "Failed to load texture: " << texturePath << std::endl;
        stbi_image_free(data);
        return false;
    }
}

void Player::Init(Math::Vec2 startPos)
{
    position = startPos;
    velocity = Math::Vec2(0.0f, 0.0f);
    m_currentGroundLevel = GROUND_LEVEL;

    std::vector<float> vertices = {
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,

        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &VAO);
    GL::GenBuffers(1, &VBO);
    GL::BindVertexArray(VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);

    LoadAnimation(AnimationState::Idle, "Asset/player_Idle.png", 10, 0.1f);
    LoadAnimation(AnimationState::Walking, "Asset/player_Walking.png", 7, 0.1f);
    LoadAnimation(AnimationState::Crouching, "Asset/player_Crouch.png", 2, 0.1f);

    AnimationData& walkAnim = m_animations[static_cast<int>(AnimationState::Walking)];
    float desiredWidth = 240.0f;
    float frameAspectRatio = static_cast<float>(walkAnim.texHeight) / static_cast<float>(walkAnim.frameWidth);
    size = Math::Vec2(desiredWidth, desiredWidth * frameAspectRatio);
    original_size = size;
}

AnimationState Player::DetermineAnimationState() const
{
    if (is_dashing)
        return AnimationState::Walking;

    if (velocity.x != 0.0f)
        return AnimationState::Walking;

    return AnimationState::Idle;
}

void Player::Update(double dt, Input::Input& input)
{
    if (IsDead())
    {
        velocity = { 0.0f, 0.0f };
        return;
    }

    if (input.IsKeyPressed(Input::Key::A)) MoveLeft();
    if (input.IsKeyPressed(Input::Key::D)) MoveRight();
    if (input.IsKeyPressed(Input::Key::Space)) Jump();
    if (input.IsKeyPressed(Input::Key::S)) Crouch();
    else StopCrouch();
    if (input.IsKeyPressed(Input::Key::LeftShift)) Dash();

    if (m_isInvincible)
    {
        m_invincibilityTimer -= static_cast<float>(dt);
        if (m_invincibilityTimer <= 0.0f)
        {
            m_isInvincible = false;
        }
    }

    if (is_dashing)
    {
        dash_timer -= static_cast<float>(dt);
        if (dash_timer <= 0.0f)
        {
            is_dashing = false;
        }
    }

    if (!is_on_ground)
    {
        velocity.y += GRAVITY * static_cast<float>(dt);
    }

    Math::Vec2 final_velocity = velocity;
    if (is_dashing)
    {
        final_velocity.x = last_move_direction * dash_speed;
    }

    position += final_velocity * static_cast<float>(dt);

    if (position.y - size.y / 2.0f <= m_currentGroundLevel)
    {
        position.y = m_currentGroundLevel + size.y / 2.0f;
        if (velocity.y < 0)
        {
            velocity.y = 0;
        }
        is_on_ground = true;
    }

    if (is_crouching)
    {
        m_currentAnimState = AnimationState::Crouching;

        if (!m_crouchAnimationFinished)
        {
            m_animations[static_cast<int>(AnimationState::Crouching)].Update(static_cast<float>(dt));

            if (m_animations[static_cast<int>(AnimationState::Crouching)].currentFrame >= 1)
            {
                m_crouchAnimationFinished = true;
                m_animations[static_cast<int>(AnimationState::Crouching)].currentFrame = 1;
                m_animations[static_cast<int>(AnimationState::Crouching)].timer = 0.0f;
            }
        }
        else
        {
            m_animations[static_cast<int>(AnimationState::Crouching)].currentFrame = 1;
            m_animations[static_cast<int>(AnimationState::Crouching)].timer = 0.0f;
        }
    }
    else if (!is_on_ground)
    {
        m_currentAnimState = AnimationState::Walking;
        m_animations[static_cast<int>(AnimationState::Walking)].currentFrame = 4;
        m_animations[static_cast<int>(AnimationState::Walking)].timer = 0.0f;
    }
    else
    {
        AnimationState newState = DetermineAnimationState();
        if (newState != m_currentAnimState)
        {
            m_animations[static_cast<int>(m_currentAnimState)].Reset();
            m_currentAnimState = newState;
        }
        m_animations[static_cast<int>(m_currentAnimState)].Update(static_cast<float>(dt));
    }

    velocity.x = 0;
}

void Player::Draw(const Shader& shader) const
{
    if (m_isInvincible && !IsDead())
    {
        if (fmod(m_invincibilityTimer, 0.2f) < 0.1f)
        {
            return;
        }
    }

    Math::Vec2 drawSize = size;
    Math::Vec2 drawPosition = position;

    if (m_currentAnimState == AnimationState::Crouching)
    {
        float oldHeight = drawSize.y;
        drawSize.x *= 0.7f;
        drawSize.y *= 0.7f;

        float heightDiff = oldHeight - drawSize.y;
        drawPosition.y -= heightDiff / 2.0f;
    }
    else if (m_currentAnimState == AnimationState::Idle)
    {
        drawSize.x *= 0.7f;
    }

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(drawSize);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation(drawPosition);
    Math::Matrix model = transMatrix * scaleMatrix;

    shader.use();
    shader.setMat4("model", model);
    shader.setBool("flipX", m_is_flipped);

    const AnimationData& currentAnim = m_animations[static_cast<int>(m_currentAnimState)];

    float frame_x_offset = static_cast<float>(currentAnim.currentFrame * currentAnim.frameWidth);
    float rect_x = frame_x_offset / currentAnim.texWidth;
    float rect_y = 0.0f;
    float rect_w = static_cast<float>(currentAnim.frameWidth) / currentAnim.texWidth;
    float rect_h = 1.0f;

    shader.setVec4("spriteRect", rect_x, rect_y, rect_w, rect_h);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, currentAnim.textureID);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Player::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);

    for (int i = 0; i < 5; ++i)
    {
        if (m_animations[i].textureID != 0)
        {
            GL::DeleteTextures(1, &m_animations[i].textureID);
        }
    }
}

void Player::TakeDamage(float amount)
{
    if (m_isInvincible) return;

    m_pulseCore.getPulse().spend(amount);
    m_isInvincible = true;
    m_invincibilityTimer = m_invincibilityDuration;

    Logger::Instance().Log(Logger::Severity::Event, "Player took %.1f damage! Remaining pulse: %.1f",
        amount, m_pulseCore.getPulse().Value());
}

Math::Vec2 Player::GetHitboxSize() const
{
    if (is_crouching)
    {
        return { size.x * 0.4f, size.y * 0.5f };
    }
    else
    {
        return { size.x * 0.4f, size.y * 0.8f + 50.0f };
    }
}

Math::Vec2 Player::GetHitboxCenter() const
{
    if (is_crouching)
    {
        // 발 위치 계산 (항상 동일)
        float footY = position.y - (size.y / 2.0f);

        // 앉았을 때 히트박스 높이
        float crouchedHitboxHeight = size.y * 0.5f;

        // 발 위치 + 히트박스 절반 = 히트박스 중심
        return { position.x, footY + (crouchedHitboxHeight / 2.0f) };
    }
    else
    {
        return position;
    }
}

void Player::MoveLeft()
{
    if (is_crouching) return;
    velocity.x -= move_speed;
    last_move_direction = -1;
    m_is_flipped = true;
}

void Player::MoveRight()
{
    if (is_crouching) return;
    velocity.x += move_speed;
    last_move_direction = 1;
    m_is_flipped = false;
}

void Player::Jump()
{
    if (is_on_ground && !is_crouching)
    {
        velocity.y = jump_velocity;
        is_on_ground = false;
    }
}

void Player::Crouch()
{
    if (is_on_ground && !is_dashing && !is_crouching)
    {
        is_crouching = true;
        m_crouchAnimationFinished = false;

        m_currentAnimState = AnimationState::Crouching;
        m_animations[static_cast<int>(AnimationState::Crouching)].currentFrame = 0;
        m_animations[static_cast<int>(AnimationState::Crouching)].timer = 0.0f;
    }
}

void Player::StopCrouch()
{
    if (is_crouching)
    {
        is_crouching = false;
        m_crouchAnimationFinished = false;
    }
}

void Player::Dash()
{
    if (!is_dashing && m_pulseCore.getPulse().Value() >= m_pulseCore.getConfig().dashCost)
    {
        m_pulseCore.getPulse().spend(m_pulseCore.getConfig().dashCost);
        is_dashing = true;
        dash_timer = dash_duration;
    }
}

void Player::SetPosition(Math::Vec2 new_pos)
{
    position = new_pos;
}

bool Player::IsFacingRight() const
{
    return !m_is_flipped;
}

void Player::SetCurrentGroundLevel(float newGroundLevel)
{
    m_currentGroundLevel = newGroundLevel;
}

void Player::ResetVelocity()
{
    velocity = Math::Vec2(0.0f, 0.0f);
}

void Player::SetOnGround(bool onGround)
{
    is_on_ground = onGround;
    if (onGround)
    {
        velocity.y = 0.0f;
    }
}

bool Player::IsDead() const
{
    return m_pulseCore.getPulse().Value() <= 0;
}