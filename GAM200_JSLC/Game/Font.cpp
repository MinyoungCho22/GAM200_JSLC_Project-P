#include "Font.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <iostream>
#include <vector>

#pragma warning(push, 0)

#include <stb_image.h>
#pragma warning(pop)

void Font::Initialize(const char* texturePath, int columns, int rows)
{
    m_numCols = columns;
    m_numRows = rows;

    // 1. 텍스처 로드
    GL::GenTextures(1, &m_textureID);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);

    // 픽셀 아트 폰트이므로 GL_NEAREST를 사용합니다.
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

    if (data)
    {
        m_texWidth = width;
        m_texHeight = height;
        m_charWidth = 1.0f / m_numCols;   // UV 너비
        m_charHeight = 1.0f / m_numRows;  // UV 높이

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        GL::GenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load font texture: " << texturePath << std::endl;
    }
    stbi_image_free(data);

    // 2. VAO/VBO 생성 (모든 문자가 (-0.5, 0.5) 중심의 쿼드 하나를 공유)
    std::vector<float> vertices = {
        // positions      // texture Coords
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
    GL::BindVertexArray(0);
}

void Font::Shutdown()
{
    GL::DeleteVertexArrays(1, &VAO);
    GL::DeleteBuffers(1, &VBO);
    GL::DeleteTextures(1, &m_textureID);
}

void Font::DrawText(Shader& shader, const std::string& text, Math::Vec2 position, float size)
{
    shader.use();
    shader.setBool("flipX", false); // 텍스트는 뒤집지 않음

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_textureID);
    GL::BindVertexArray(VAO);

    Math::Vec2 currentPos = position;
    float charRenderWidth = size;
    float charRenderHeight = size; // (비율을 1:1로 가정, 필요시 수정)

    for (const char c : text)
    {
        int charIndex = static_cast<int>(c);
        if (charIndex < 0 || charIndex > 127) continue; // ASCII 범위 밖이면 무시

        // 폰트 시트에서 문자의 UV 좌표 계산
        int row = charIndex / m_numCols;
        int col = charIndex % m_numCols;

        // Y좌표는 텍스처가 위에서 아래로(top-down) 되어있다고 가정
        float uv_x = static_cast<float>(col) / m_numCols;
        float uv_y = static_cast<float>(row) / m_numRows;

        // stbi_load가 이미지를 뒤집었으므로(true), UV의 Y좌표를 다시 뒤집어줍니다.
        uv_y = (1.0f - m_charHeight) - uv_y;

        shader.setVec4("spriteRect", uv_x, uv_y, m_charWidth, m_charHeight);

        // 문자의 그리기 위치 계산 (중심점 기준)
        Math::Vec2 charCenter = { currentPos.x + charRenderWidth / 2.0f, currentPos.y + charRenderHeight / 2.0f };
        Math::Matrix model = Math::Matrix::CreateTranslation(charCenter) * Math::Matrix::CreateScale({ charRenderWidth, charRenderHeight });
        shader.setMat4("model", model);

        GL::DrawArrays(GL_TRIANGLES, 0, 6);

        // 다음 문자 위치로 이동
        currentPos.x += charRenderWidth;
    }

    GL::BindVertexArray(0);
    GL::BindTexture(GL_TEXTURE_2D, 0);
}