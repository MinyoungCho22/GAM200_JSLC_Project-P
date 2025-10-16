#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#pragma warning(push)
#pragma warning(disable: 6262)
#include <stb_image.h>
#pragma warning(pop)

const float GRAVITY = -1500.0f;
const float GROUND_LEVEL = 350.0f;

void Player::Init(Math::Vec2 startPos, const char* texturePath)
{
    position = startPos;
    velocity = Math::Vec2(0.0f, 0.0f);

    float vertices[] = {
        0.0f, 1.0f,  0.0f, 1.0f, 1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f,
        0.0f, 1.0f,  0.0f, 1.0f, 1.0f, 1.0f,  1.0f, 1.0f, 1.0f, 0.0f,  1.0f, 0.0f
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        float desiredWidth = 120.0f;
        float aspectRatio = static_cast<float>(height) / static_cast<float>(width);
        size = Math::Vec2(desiredWidth, desiredWidth * aspectRatio);
        original_size = size;
    }
    else { std::cout << "Failed to load texture: " << texturePath << std::endl; }
    stbi_image_free(data);
}

void Player::Update(double dt)
{
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
    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(size);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation({ position.x, position.y + size.y / 2.0f });
    Math::Matrix model = transMatrix * scaleMatrix;

    shader.setMat4("model", model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Player::Shutdown()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
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
        // [추가] 펄스를 실제로 소모하는 코드
        m_pulseCore.getPulse().spend(m_pulseCore.getConfig().dashCost);

        is_dashing = true;
        dash_timer = dash_duration;
    }
}