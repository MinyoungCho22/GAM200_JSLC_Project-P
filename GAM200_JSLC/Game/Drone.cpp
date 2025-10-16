#include "Drone.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include <glad/glad.h>
#include <stb_image.h>
#include <cmath>

void Drone::Init(Math::Vec2 startPos, const char* texturePath)
{
    m_position = startPos;
    m_velocity = { 0.0f, 0.0f };
    m_direction = { 1.0f, 0.0f };

    float vertices[] = { 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f };
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
        float desiredWidth = 60.0f;
        float aspectRatio = static_cast<float>(height) / static_cast<float>(width);
        m_size = { desiredWidth, desiredWidth * aspectRatio };
        Logger::Instance().Log(Logger::Severity::Info, "Drone texture loaded: %s", texturePath);
    }
    else { Logger::Instance().Log(Logger::Severity::Error, "Failed to load drone texture: %s", texturePath); }
    stbi_image_free(data);
}

void Drone::Update(double dt)
{
    // [수정] 피격 상태일 때의 로직
    if (m_isHit)
    {
        m_hitTimer -= static_cast<float>(dt);
        if (m_hitTimer <= 0.0f)
        {
            m_isHit = false;
        }
        return; // 피격 시에는 움직이지 않음
    }

    m_moveTimer += static_cast<float>(dt);
    if (m_moveTimer > 3.0f)
    {
        m_moveTimer = 0.0f;
        m_direction.x = -m_direction.x;
    }
    m_velocity = m_direction * m_speed;
    m_position += m_velocity * static_cast<float>(dt);
}

void Drone::Draw(const Shader& shader) const
{
    Math::Matrix rotationMatrix = Math::Matrix::CreateIdentity();
    // [수정] 피격 상태일 때 흔들리는 효과 적용
    if (m_isHit)
    {
        // sin 함수를 이용해 좌우로 부드럽게 흔들리게 함
        float angle = std::sin(m_hitTimer * 30.0f) * 45.0f;
        rotationMatrix = Math::Matrix::CreateRotation(angle);
    }

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(m_size);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation({ m_position.x, m_position.y + m_size.y / 2.0f });
    Math::Matrix model = transMatrix * rotationMatrix * scaleMatrix;

    shader.setMat4("model", model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Drone::Shutdown()
{
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &textureID);
}

// [추가] TakeHit 함수 구현
void Drone::TakeHit()
{
    if (m_isHit) return; // 이미 피격 상태면 무시
    m_isHit = true;
    m_hitTimer = m_hitDuration;
}