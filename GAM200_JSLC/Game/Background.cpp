#include "Background.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/Shader.hpp"
#include <stb_image.h>
#include <iostream>

void Background::Initialize(const char* texturePath)
{
    // 1. 텍스처 로드 (stb_image 사용)
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (!data)
    {
        std::cout << "Failed to load background texture: " << texturePath << std::endl;
        return;
    }

    // 2. OpenGL 텍스처 생성
    GL::GenTextures(1, &m_textureID);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    GL::GenerateMipmap(GL_TEXTURE_2D);

    // 배경 이미지는 반복되지 않고 가장자리에 맞춰지도록 설정
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);

    // 3. VAO/VBO 생성 (중앙 기준 사각형)
    // 렌더링에 사용할 사각형 정점 (위치 2f, 텍스처 좌표 2f)
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
    // 모델 행렬을 셰이더에 전달
    shader.setMat4("model", model);

    // 텍스처를 활성화하고 바인딩
    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    // VAO를 바인딩하고 사각형 그리기
    GL::BindVertexArray(VAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    // 바인딩 해제
    GL::BindVertexArray(0);
}