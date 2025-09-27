#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

// stb_image.h 라이브러리의 실제 구현부를 생성하는 매크로
// 이 매크로는 프로젝트 전체에서 단 하나의 .cpp 파일에만 존재해야 함
// 여러 파일에 포함되면 '중복 정의' 링크 오류가 발생!
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 6262) // 과도한 스택 사용 경고 비활성화(추후 수정 예정)

#include <stb_image.h>
#pragma warning(pop)


const float GRAVITY = -1500.0f; // 중력

void Player::Init(Math::Vec2 startPos, const char* texturePath)
{
    position = startPos;
    velocity = Math::Vec2(0.0f, 0.0f);

    float vertices[] = {
        // positions // texture Coords
        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f,
        0.0f, 0.0f,  0.0f, 0.0f,

        0.0f, 1.0f,  0.0f, 1.0f,
        1.0f, 1.0f,  1.0f, 1.0f,
        1.0f, 0.0f,  1.0f, 0.0f
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

        float desiredWidth = 80.0f;
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

    if (position.y < 100.0f)
    {
        position.y = 100.0f;
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
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation({ position.x - size.x / 2.0f, position.y });
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
    if (!is_dashing)
    {
        is_dashing = true;
        dash_timer = dash_duration;
    }
}