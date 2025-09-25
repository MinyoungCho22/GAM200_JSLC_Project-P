#include "Player.h"
#include "../Engine/Vec2.h"
#include "../OpenGL/Shader.h"
#include "../Engine/Matrix.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable: 6262) // 과도한 스택 사용 경고 비활성화
#include <stb_image.h>
#pragma warning(pop)
#include <iostream>

const float GRAVITY = -980.0f;

void Player::Init(Vec2 startPos, const char* texturePath)
{
    position = startPos;
    velocity = Vec2(0.0f, 0.0f);

    float vertices[] = {
        0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);
    if (data)
    {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // --- 수정: 이미지 비율 유지하며 크기 설정 ---
        float desiredWidth = 80.0f;
        float aspectRatio = static_cast<float>(height) / static_cast<float>(width);
        size = Vec2(desiredWidth, desiredWidth * aspectRatio);
        original_size = size; // 원래 크기 저장
    }
    else { std::cout << "Failed to load texture: " << texturePath << std::endl; }
    stbi_image_free(data);
}

void Player::Update(double dt, GLFWwindow* p_window)
{
    // --- 수정: 중력 계산을 항상 적용하도록 변경 ---
    // 1. 점프, 웅크리기 등 대시가 아닐 때의 로직 처리
    if (!is_dashing)
    {
        if (is_on_ground && glfwGetKey(p_window, GLFW_KEY_S) == GLFW_PRESS) {
            is_crouching = true;
            size.y = original_size.y * 0.6f;
        }
        else {
            is_crouching = false;
            size.y = original_size.y;
        }

        if (!is_crouching)
        {
            velocity.x = 0;
            if (glfwGetKey(p_window, GLFW_KEY_A) == GLFW_PRESS) {
                velocity.x -= move_speed;
                last_move_direction = -1;
            }
            if (glfwGetKey(p_window, GLFW_KEY_D) == GLFW_PRESS) {
                velocity.x += move_speed;
                last_move_direction = 1;
            }

            if (is_on_ground && glfwGetKey(p_window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                velocity.y = jump_velocity;
                is_on_ground = false;
            }
        }
        else {
            velocity.x = 0;
        }

        if (glfwGetKey(p_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            is_dashing = true;
            dash_timer = dash_duration;
        }
    }

    // 2. 대시 타이머 업데이트 및 상태 관리
    if (is_dashing)
    {
        dash_timer -= static_cast<float>(dt);
        if (dash_timer <= 0.0f) {
            is_dashing = false;
            // 대시가 끝나도 수직 속도는 유지되도록 velocity.y = 0; 줄 삭제
        }
    }

    // 3. 물리 계산 (언제나 중력 적용)
    velocity.y += GRAVITY * static_cast<float>(dt);

    // 4. 최종 위치 업데이트
    Vec2 final_velocity = velocity;
    if (is_dashing) {
        // 대시 중에는 수평 속도를 덮어씀
        final_velocity.x = last_move_direction * dash_speed;
    }

    position += final_velocity * static_cast<float>(dt);

    // 5. 바닥 충돌 처리
    if (position.y < 100.0f)
    {
        position.y = 100.0f;
        velocity.y = 0;
        is_on_ground = true;
    }
}


void Player::Draw(const Shader& shader) const
{
    Matrix scaleMatrix = Matrix::CreateScale(size);
    Matrix transMatrix = Matrix::CreateTranslation(position);
    Matrix model = transMatrix * scaleMatrix;

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