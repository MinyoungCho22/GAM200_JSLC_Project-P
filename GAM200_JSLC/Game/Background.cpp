#include "Background.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/Shader.hpp"
#include <iostream>

#pragma warning(push, 0)
// Player.cpp에 IMPLEMENTATION이 있으므로 여기서는 #define을 삭제합니다.
#include <stb_image.h>
#pragma warning(pop)

void Background::Initialize(const char* texturePath)
{
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (!data)
    {
        std::cout << "Failed to load background texture: " << texturePath << std::endl;
        return;
    }

    // 원본 이미지 크기를 멤버 변수에 저장
    m_imageWidth = width;
    m_imageHeight = height;

    // OpenGL 텍스처 생성
    GL::GenTextures(1, &m_textureID);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    GL::GenerateMipmap(GL_TEXTURE_2D);

    // 텍스처 필터링 및 래핑 설정
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    // VAO/VBO 생성 (중앙 기준 사각형)
    float vertices[] = {
        // positions    // texture Coords
        -0.5f,  0.5f,   0.0f, 1.0f, // Top-left
         0.5f, -0.5f,   1.0f, 0.0f, // Bottom-right
        -0.5f, -0.5f,   0.0f, 0.0f, // Bottom-left

        -0.5f,  0.5f,   0.0f, 1.0f, // Top-left
         0.5f,  0.5f,   1.0f, 1.0f, // Top-right
         0.5f, -0.5f,   1.0f, 0.0f  // Bottom-right
    };

    GL::GenVertexArrays(1, &VAO);
    GL::GenBuffers(1, &VBO);
    GL::BindVertexArray(VAO);

    GL::BindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    // Texture coord attribute
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);

    GL::BindVertexArray(0);
}

void Background::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteTextures(1, &m_textureID);
}

void Background::Draw(Shader& shader, const Math::Matrix& model)
{
    // shader.use()는 GameplayState::Draw()에서 이미 호출됨

    shader.setMat4("model", model);

    // 배경은 스프라이트 시트가 아니므로, 유니폼을 '전체 텍스처'로 리셋
    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", false);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    GL::BindVertexArray(0);
}