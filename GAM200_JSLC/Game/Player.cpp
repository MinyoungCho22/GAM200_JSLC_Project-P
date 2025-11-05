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
#pragma warning(pop)

const float GRAVITY = -1500.0f;
const float GROUND_LEVEL = 60.0f;

void Player::Init(Math::Vec2 startPos, const char* texturePath)
{
    position = startPos;
    velocity = Math::Vec2(0.0f, 0.0f);

    std::vector<float> vertices = {
        // VBO 정점 데이터는 0.0~1.0 범위를 유지합니다.
        // 셰이더에서 UV를 변환할 것이므로 수정할 필요가 없습니다.
        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 0.0f,

        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f
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

    GL::GenTextures(1, &textureID);
    GL::BindTexture(GL_TEXTURE_2D, textureID);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);
    if (data)
    {
      
        m_tex_width = width;   // 전체 시트 너비
        m_tex_height = height; // 전체 시트 높이
        m_frame_width = width / m_anim_total_frames; // 1 프레임 너비

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);

        float desiredWidth = 360.0f;

       
        float frameAspectRatio = static_cast<float>(m_tex_height) / static_cast<float>(m_frame_width);
        size = Math::Vec2(desiredWidth, desiredWidth * frameAspectRatio);
        original_size = size;
    }
    else {
        std::cout << "Failed to load texture: " << texturePath << std::endl;
    }
    stbi_image_free(data);
}


void Player::Update(double dt)
{
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
    if (is_dashing) {
        final_velocity.x = last_move_direction * dash_speed;
    }

    position += final_velocity * static_cast<float>(dt);

    if (position.y < GROUND_LEVEL)
    {
        position.y = GROUND_LEVEL;
        if (velocity.y < 0)
        {
            velocity.y = 0;
            is_on_ground = true;
        }
    }

    
    if (velocity.x != 0.0f && is_on_ground && !is_crouching && !is_dashing) // 걷는 중
    {
        m_anim_timer += static_cast<float>(dt);
        if (m_anim_timer >= m_anim_frame_duration)
        {
            m_anim_timer -= m_anim_frame_duration;
            m_anim_current_frame = (m_anim_current_frame + 1) % m_anim_total_frames;
        }
    }
    else // 멈춤, 점프, 대시 등
    {
        m_anim_current_frame = 0; // 멈추면 첫 번째 프레임(Idle)으로
        m_anim_timer = 0.0f;
    }
    // --- 애니메이션 로직 끝 ---

    velocity.x = 0; // 속도는 매 프레임 입력에 따라 새로 계산됨
}

void Player::Draw(const Shader& shader) const
{
    if (m_isInvincible)
    {
        if (fmod(m_invincibilityTimer, 0.2f) < 0.1f)
        {
            return;
        }
    }

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(size);
    //
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation({ position.x + size.x / 2.0f, position.y + size.y / 2.0f });
    Math::Matrix model = transMatrix * scaleMatrix;

    shader.use(); //
    shader.setMat4("model", model);

    // --- 
    // 1. 현재 프레임의 텍스처 내 X 시작 위치 (0.0 ~ 1.0)
    float frame_x_offset = static_cast<float>(m_anim_current_frame * m_frame_width);
    float rect_x = frame_x_offset / m_tex_width;
    // 2. 텍스처 Y 시작 위치 (항상 0)
    float rect_y = 0.0f;
    // 3. 1개 프레임의 너비 비율 (0.0 ~ 1.0)
    float rect_w = static_cast<float>(m_frame_width) / m_tex_width;
    // 4. 1개 프레임의 높이 비율 (항상 1.0)
    float rect_h = 1.0f;

    shader.setVec4("spriteRect", rect_x, rect_y, rect_w, rect_h);
    shader.setBool("flipX", m_is_flipped);
    // --- 유니폼 설정 끝 ---

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureID);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Player::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteTextures(1, &textureID);
}

void Player::TakeDamage(float amount)
{
    if (m_isInvincible) return;

    m_pulseCore.getPulse().spend(amount);
    m_isInvincible = true;
    m_invincibilityTimer = m_invincibilityDuration;
}

void Player::MoveLeft()
{
    if (is_crouching || is_dashing) return;
    velocity.x -= move_speed;
    last_move_direction = -1;
    m_is_flipped = true; 
}

void Player::MoveRight()
{
    if (is_crouching || is_dashing) return;
    velocity.x += move_speed;
    last_move_direction = 1;
    m_is_flipped = false; 
}

void Player::Jump()
{
    if (is_on_ground && !is_crouching && !is_dashing)
    {
        velocity.y = jump_velocity;
        is_on_ground = false;
    }
}

void Player::Crouch()
{
    if (is_on_ground && !is_dashing)
    {
        is_crouching = true;
        size.y = original_size.y * 0.6f;
    }
}

void Player::StopCrouch()
{
    is_crouching = false;
    size.y = original_size.y;
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