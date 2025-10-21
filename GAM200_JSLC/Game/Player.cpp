﻿#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

const float GRAVITY = -1500.0f;
const float GROUND_LEVEL = 350.0f;

void Player::Init(Math::Vec2 startPos, const char* texturePath)
{
    position = startPos;
    velocity = Math::Vec2(0.0f, 0.0f);

    std::vector<float> vertices = {
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
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);

        float desiredWidth = 120.0f;
        float aspectRatio = static_cast<float>(height) / static_cast<float>(width);
        size = Math::Vec2(desiredWidth, desiredWidth * aspectRatio);
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
    velocity.x = 0;
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
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation({ position.x, position.y + size.y / 2.0f });
    Math::Matrix model = transMatrix * scaleMatrix;

    shader.setMat4("model", model);

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
}

void Player::MoveRight()
{
    if (is_crouching || is_dashing) return;
    velocity.x += move_speed;
    last_move_direction = 1;
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