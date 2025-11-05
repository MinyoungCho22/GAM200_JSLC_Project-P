#include "Drone.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <cmath>

#pragma warning(push, 0)
// ✅ [수정] #define STB_IMAGE_IMPLEMENTATION 라인을 삭제
#include <stb_image.h>
#pragma warning(pop)

void Drone::Init(Math::Vec2 startPos, const char* texturePath)
{
    m_position = startPos;
    m_velocity = { 0.0f, 0.0f };
    m_direction = { 1.0f, 0.0f };

    float vertices[] = {
        -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f, -0.5f,     1.0f, 0.0f,
        -0.5f, -0.5f,     0.0f, 0.0f,

        -0.5f,  0.5f,     0.0f, 1.0f,
         0.5f,  0.5f,     1.0f, 1.0f,
         0.5f, -0.5f,     1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &VAO);
    GL::GenBuffers(1, &VBO);
    GL::BindVertexArray(VAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
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
    if (m_isHit)
    {
        m_hitTimer -= static_cast<float>(dt);
        if (m_hitTimer <= 0.0f)
        {
            m_isHit = false;
        }
        return;
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
    if (m_isHit)
    {
        float angle = std::sin(m_hitTimer * 30.0f) * 45.0f;
        rotationMatrix = Math::Matrix::CreateRotation(angle);
    }

    Math::Matrix scaleMatrix = Math::Matrix::CreateScale(m_size);
    Math::Matrix transMatrix = Math::Matrix::CreateTranslation(m_position);
    Math::Matrix model = transMatrix * rotationMatrix * scaleMatrix;

    shader.use();
    shader.setMat4("model", model);

    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", m_direction.x < 0.0f);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureID);
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}

void Drone::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteTextures(1, &textureID);
}

void Drone::TakeHit()
{
    if (m_isHit) return;
    m_isHit = true;
    m_hitTimer = m_hitDuration;
}