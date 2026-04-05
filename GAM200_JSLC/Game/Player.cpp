//Player.cpp

#include "Player.hpp"
#include "../Engine/ControlBindings.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

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
    can_double_jump = false;
    is_double_jumping = false;

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

    LoadAnimation(AnimationState::Idle, "Asset/Player_Idle.png", 10, 0.1f);
    LoadAnimation(AnimationState::Walking, "Asset/Player_Walking.png", 7, 0.1f);
    LoadAnimation(AnimationState::Crouching, "Asset/Player_Crouch.png", 2, 0.1f);

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

void Player::Update(double dt, Input::Input& input, const ControlBindings& controls)
{
    const float fdt = static_cast<float>(dt);

    if (IsDead())
    {
        velocity = { 0.0f, 0.0f };
        m_currentHorizontalSpeed = 0.0f;
        return;
    }

    if (velocity.y < 0.0f)
    {
        is_on_ground = false;
    }

    const float moveAxis = controls.GetMoveHorizontalAxis(input);

    if (controls.IsActionTriggered(ControlAction::Jump, input)) Jump();

    if (input.IsCrouchHeld()) Crouch();
    else StopCrouch();
    if (controls.IsActionPressed(ControlAction::Dash, input)) Dash();

    if (m_isInvincible)
    {
        m_invincibilityTimer -= fdt;
        if (m_invincibilityTimer <= 0.0f)
        {
            m_isInvincible = false;
        }
    }

    if (is_dashing)
    {
        dash_timer -= fdt;
        if (dash_timer <= 0.0f)
        {
            is_dashing = false;
        }
    }
    else
    {
        m_afterimageSpawnTimer = 0.0f;
    }

    // Fade out existing ghosts and remove expired ones
    for (auto& ghost : m_afterimageGhosts)
    {
        ghost.alpha -= AFTERIMAGE_FADE_SPEED * fdt;
    }
    m_afterimageGhosts.erase(
        std::remove_if(m_afterimageGhosts.begin(), m_afterimageGhosts.end(),
            [](const AfterimageGhost& g) { return g.alpha <= 0.0f; }),
        m_afterimageGhosts.end());

    // Horizontal movement with acceleration/deceleration.
    if (!is_dashing)
    {
        if (moveAxis != 0.0f && !is_crouching)
        {
            float targetSpeed = moveAxis * m_maxSpeed;
            if (m_currentHorizontalSpeed < targetSpeed)
                m_currentHorizontalSpeed = (std::min)(targetSpeed, m_currentHorizontalSpeed + m_acceleration * fdt);
            else if (m_currentHorizontalSpeed > targetSpeed)
                m_currentHorizontalSpeed = (std::max)(targetSpeed, m_currentHorizontalSpeed - m_acceleration * fdt);

            if (moveAxis < 0.0f)
            {
                last_move_direction = -1;
                m_is_flipped = true;
            }
            else if (moveAxis > 0.0f)
            {
                last_move_direction = 1;
                m_is_flipped = false;
            }
        }
        else
        {
            if (m_currentHorizontalSpeed > 0.0f)
                m_currentHorizontalSpeed = (std::max)(0.0f, m_currentHorizontalSpeed - m_friction * fdt);
            else if (m_currentHorizontalSpeed < 0.0f)
                m_currentHorizontalSpeed = (std::min)(0.0f, m_currentHorizontalSpeed + m_friction * fdt);
        }

        velocity.x = m_currentHorizontalSpeed;
    }

    velocity.y += GRAVITY * fdt;

    Math::Vec2 final_velocity = velocity;
    if (is_dashing)
    {
        final_velocity.x = last_move_direction * dash_speed;
    }

    position += final_velocity * fdt;

    if (position.y - size.y / 2.0f <= m_currentGroundLevel)
    {
        position.y = m_currentGroundLevel + size.y / 2.0f;
        if (velocity.y < 0)
        {
            velocity.y = 0;
        }
        is_on_ground = true;
        is_double_jumping = false;
        can_double_jump = false;
    }


    if (is_crouching)
    {
        m_currentAnimState = AnimationState::Crouching;

        if (!m_crouchAnimationFinished)
        {
            m_animations[static_cast<int>(AnimationState::Crouching)].Update(fdt);

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


        int airFrame = is_double_jumping ? 5 : 4;

        m_animations[static_cast<int>(AnimationState::Walking)].currentFrame = airFrame;
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
        m_animations[static_cast<int>(m_currentAnimState)].Update(fdt);
    }

    // Spawn Sandevistan afterimage ghosts at fixed intervals.
    // Spawn after animation-state update so crouch-dash leaves crouching ghosts.
    if (is_dashing)
    {
        m_afterimageSpawnTimer -= fdt;
        if (m_afterimageSpawnTimer <= 0.0f)
        {
            m_afterimageSpawnTimer = AFTERIMAGE_INTERVAL;
            AfterimageGhost ghost;
            ghost.position = position;
            ghost.animState = m_currentAnimState;
            ghost.animFrame = m_animations[static_cast<int>(m_currentAnimState)].currentFrame;
            ghost.flipped = m_is_flipped;
            ghost.alpha = AFTERIMAGE_INIT_ALPHA;
            m_afterimageGhosts.push_back(ghost);
        }
    }

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

    // Draw Sandevistan afterimage ghosts before the player (they appear behind)
    if (!m_afterimageGhosts.empty())
    {
        // Cyberpunk electric cyan tint
        shader.setVec3("colorTint", 0.0f, 0.85f, 1.0f);

        for (const auto& ghost : m_afterimageGhosts)
        {
            AnimationState ghostState = ghost.animState;
            const AnimationData* ghostAnim = &m_animations[static_cast<int>(ghostState)];
            if (ghostAnim->textureID == 0 || ghostAnim->totalFrames <= 0)
            {
                ghostState = AnimationState::Walking;
                ghostAnim = &m_animations[static_cast<int>(AnimationState::Walking)];
            }

            Math::Vec2 ghostSize = size;
            Math::Vec2 ghostPos = ghost.position;
            if (ghostState == AnimationState::Crouching)
            {
                float oldHeight = ghostSize.y;
                ghostSize.x *= 0.7f;
                ghostSize.y *= 0.7f;
                ghostPos.y -= (oldHeight - ghostSize.y) * 0.5f;
            }
            else if (ghostState == AnimationState::Idle)
            {
                ghostSize.x *= 0.7f;
            }

            Math::Matrix ghostModel = Math::Matrix::CreateTranslation(ghostPos) *
                                      Math::Matrix::CreateScale(ghostSize);
            shader.setMat4("model", ghostModel);
            shader.setBool("flipX", ghost.flipped);
            shader.setFloat("alpha", ghost.alpha);
            shader.setFloat("tintStrength", 0.75f);

            int safeFrame = ghost.animFrame;
            if (safeFrame < 0) safeFrame = 0;
            if (safeFrame >= ghostAnim->totalFrames) safeFrame = ghostAnim->totalFrames - 1;

            float frame_x = static_cast<float>(safeFrame * ghostAnim->frameWidth);
            float rect_x  = frame_x / static_cast<float>(ghostAnim->texWidth);
            float rect_w  = static_cast<float>(ghostAnim->frameWidth) / static_cast<float>(ghostAnim->texWidth);
            shader.setVec4("spriteRect", rect_x, 0.0f, rect_w, 1.0f);

            GL::ActiveTexture(GL_TEXTURE0);
            GL::BindTexture(GL_TEXTURE_2D, ghostAnim->textureID);
            GL::BindVertexArray(VAO);
            GL::DrawArrays(GL_TRIANGLES, 0, 6);
            GL::BindVertexArray(0);
        }

        // Reset tint state before drawing the main player sprite
        shader.setFloat("tintStrength", 0.0f);
    }

    Math::Vec2 drawSize{};
    Math::Vec2 drawPosition{};
    GetCurrentDrawTransform(drawPosition, drawSize);

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(drawSize);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation(drawPosition);
    Math::Matrix model = transMatrix * scaleMatrix;

    shader.use();
    shader.setMat4("model", model);
    shader.setBool("flipX", m_is_flipped);

    if (m_isHiding)
    {
        shader.setFloat("alpha", 0.5f);
    }
    else
    {
        shader.setFloat("alpha", 1.0f);
    }

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

    shader.setFloat("alpha", 1.0f);
    shader.setFloat("tintStrength", 0.0f);
}

void Player::DrawOutline(const Shader& outlineShader) const
{
    if (IsDead()) return;

    const AnimationData& currentAnim = m_animations[static_cast<int>(m_currentAnimState)];
    if (currentAnim.textureID == 0 || currentAnim.texWidth <= 0 || currentAnim.texHeight <= 0)
    {
        return;
    }

    Math::Vec2 drawSize{};
    Math::Vec2 drawPosition{};
    GetCurrentDrawTransform(drawPosition, drawSize);

    Math::Matrix model = Math::Matrix::CreateTranslation(drawPosition) * Math::Matrix::CreateScale(drawSize);

    outlineShader.use();
    outlineShader.setMat4("model", model);
    outlineShader.setBool("flipX", m_is_flipped);
    outlineShader.setFloat("alpha", m_isHiding ? 0.5f : 1.0f);
    outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f);
    outlineShader.setFloat("outlineWidthTexels", 2.0f);
    outlineShader.setVec2("texelSize", 1.0f / static_cast<float>(currentAnim.frameWidth),
                          1.0f / static_cast<float>(currentAnim.texHeight));

    float frame_x_offset = static_cast<float>(currentAnim.currentFrame * currentAnim.frameWidth);
    float rect_x = frame_x_offset / static_cast<float>(currentAnim.texWidth);
    float rect_w = static_cast<float>(currentAnim.frameWidth) / static_cast<float>(currentAnim.texWidth);
    outlineShader.setVec4("spriteRect", rect_x, 0.0f, rect_w, 1.0f);

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
    if (m_isInvincible || m_godMode) return;

    m_pulseCore.getPulse().spend(amount);
    m_isInvincible = true;
    m_invincibilityTimer = m_invincibilityDuration;

    Logger::Instance().Log(Logger::Severity::Event, "Player took %.1f damage! Remaining pulse: %.1f",
        amount, m_pulseCore.getPulse().Value());
}

Math::Vec2 Player::GetHitboxSize() const
{
    Math::Vec2 drawSize{};
    Math::Vec2 drawPosition{};
    GetCurrentDrawTransform(drawPosition, drawSize);

    if (is_crouching)
    {
        return { drawSize.x * 0.75f, drawSize.y * 0.9f };
    }
    return { drawSize.x * 0.7f, drawSize.y * 0.92f };
}

Math::Vec2 Player::GetHitboxCenter() const
{
    Math::Vec2 drawSize{};
    Math::Vec2 drawPosition{};
    GetCurrentDrawTransform(drawPosition, drawSize);

    Math::Vec2 currentHitboxSize = GetHitboxSize();
    float spriteFootY = drawPosition.y - (drawSize.y / 2.0f);
    float hitboxCenterY = spriteFootY + (currentHitboxSize.y / 2.0f);

    return { drawPosition.x, hitboxCenterY };
}

void Player::GetCurrentDrawTransform(Math::Vec2& drawPosition, Math::Vec2& drawSize) const
{
    drawSize = size;
    drawPosition = position;

    if (m_currentAnimState == AnimationState::Crouching)
    {
        float oldHeight = drawSize.y;
        drawSize.x *= 0.7f;
        drawSize.y *= 0.7f;
        drawPosition.y -= (oldHeight - drawSize.y) * 0.5f;
    }
    else if (m_currentAnimState == AnimationState::Idle)
    {
        drawSize.x *= 0.7f;
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
        can_double_jump = true;
        is_double_jumping = false;
    }
    else if (can_double_jump && !is_crouching)
    {
        velocity.y = jump_velocity;
        can_double_jump = false;
        is_double_jumping = true;
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
    constexpr float MIN_PULSE_TO_ALLOW_DASH = 10.0f;
    const float currentPulse = m_pulseCore.getPulse().Value();
    const float dashCost = m_pulseCore.getConfig().dashCost;

    if (!is_dashing && currentPulse > MIN_PULSE_TO_ALLOW_DASH && currentPulse >= dashCost)
    {
        m_pulseCore.getPulse().spend(dashCost);
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