#include "Font.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include <vector>
#include <iostream>

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

unsigned int Font::GetPixel(const unsigned char* data, int x, int y, int width, int channels) const
{
    // stbi_load(false) 기준. (0,0)은 Top-Left
    const unsigned char* p = data + (y * width + x) * channels;
    unsigned int r = p[0];
    unsigned int g = p[1];
    unsigned int b = p[2];
    unsigned int a = (channels == 4) ? p[3] : 255;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

void Font::Initialize(const char* fontAtlasPath)
{
    // stbi_load(false) (엔진의 다른 텍스처 로딩과 일관성 유지)
    stbi_set_flip_vertically_on_load(false);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(fontAtlasPath, &width, &height, &nrChannels, 0);
    if (!data)
    {
        std::cerr << "Failed to load font texture: " << fontAtlasPath << std::endl;
        return;
    }

    m_atlasWidth = width;
    m_atlasHeight = height;

    if (GetPixel(data, 0, 0, width, nrChannels) != 0xFFFFFFFF)
    {
        std::cerr << "Font Error: Font image has invalid format. First pixel must be white." << std::endl;
        stbi_image_free(data);
        return;
    }

    m_fontHeight = height - 1; // y=0은 파싱용, y=1 ~ (height)가 폰트 높이
    size_t current_char_index = 0;
    int last_boundary_x = 0;
    unsigned int last_color = GetPixel(data, 0, 0, width, nrChannels); // y=0 라인 스캔

    for (int x = 1; x < width; ++x)
    {
        unsigned int current_color = GetPixel(data, x, 0, width, nrChannels); // y=0 라인
        if (current_color != last_color)
        {
            if (current_char_index >= m_charSequence.length())
            {
                std::cerr << "Font Error: Font image has more characters than expected." << std::endl;
                break;
            }

            char c = m_charSequence[current_char_index];
            Math::IRect rect;
            // 픽셀 좌표 (y=0 Top, y=height Bottom) 기준
            rect.bottom_left = { last_boundary_x, 1 };                 // (min_x, min_y) (min_y = 1)
            rect.top_right = { x, 1 + m_fontHeight };                // (max_x, max_y) (max_y = height)
            m_charToRectMap[c] = rect;

            current_char_index++;
            last_boundary_x = x;
            last_color = current_color;
        }
    }

    // --- OpenGL 텍스처/VAO/FBO 생성 ---

    GL::GenTextures(1, &m_atlasTextureID);
    GL::BindTexture(GL_TEXTURE_2D, m_atlasTextureID);

    GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
    GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    // GL_NEAREST는 여기서 사용
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    stbi_image_free(data);

    std::vector<float> vertices = {
        // positions   // texture Coords
        -0.5f,  0.5f,  0.0f, 1.0f, // Top-left
         0.5f, -0.5f,  1.0f, 0.0f, // Bottom-right
        -0.5f, -0.5f,  0.0f, 0.0f, // Bottom-left

        -0.5f,  0.5f,  0.0f, 1.0f, // Top-left
         0.5f,  0.5f,  1.0f, 1.0f, // Top-right
         0.5f, -0.5f,  1.0f, 0.0f  // Bottom-right
    };

    GL::GenVertexArrays(1, &m_quadVAO);
    GL::GenBuffers(1, &m_quadVBO);
    GL::BindVertexArray(m_quadVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    GL::BufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);

    GL::GenFramebuffers(1, &m_fboID);
}

void Font::Shutdown()
{
    // 캐시된 모든 텍스처 삭제
    for (auto const& [key, val] : m_textCache)
    {
        GL::DeleteTextures(1, &val.textureID);
    }
    m_textCache.clear();

    // FBO 삭제
    if (m_fboID != 0) GL::DeleteFramebuffers(1, &m_fboID);

    // 쿼드 VAO/VBO 삭제
    if (m_quadVAO != 0) GL::DeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO != 0) GL::DeleteBuffers(1, &m_quadVBO);

    // 원본 아틀라스 텍스처 삭제
    if (m_atlasTextureID != 0) GL::DeleteTextures(1, &m_atlasTextureID);
}

CachedTextureInfo Font::PrintToTexture(Shader& atlasShader, const std::string& text)
{
    if (text.empty())
    {
        return { 0, 0, 0 };
    }

    auto it = m_textCache.find(text);
    if (it != m_textCache.end())
    {
        return it->second;
    }

    CachedTextureInfo newTextureInfo = BakeTextToTexture(atlasShader, text);
    newTextureInfo.text = text;

    m_textCache[text] = newTextureInfo;
    return newTextureInfo;
}

CachedTextureInfo Font::BakeTextToTexture(Shader& atlasShader, const std::string& text)
{
    Math::ivec2 textSize = measureText(text);
    if (textSize.x == 0)
    {
        return { 0, 0, 0 };
    }

    unsigned int newTexID = 0;
    GL::GenTextures(1, &newTexID);
    GL::BindTexture(GL_TEXTURE_2D, newTexID);
    GL::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textSize.x, textSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL::BindFramebuffer(GL_FRAMEBUFFER, m_fboID);
    GL::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, newTexID, 0);

    if (GL::CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer is not complete!" << std::endl;
        GL::BindFramebuffer(GL_FRAMEBUFFER, 0);
        return { 0, 0, 0 };
    }

    GL::Viewport(0, 0, textSize.x, textSize.y);
    GL::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    GL::Clear(GL_COLOR_BUFFER_BIT);

    atlasShader.use();
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(textSize.x), 0.0f, static_cast<float>(textSize.y), -1.0f, 1.0f);
    atlasShader.setMat4("projection", projection);
    atlasShader.setBool("flipX", false);

    atlasShader.setFloat("alpha", 1.0f);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, m_atlasTextureID);
    GL::BindVertexArray(m_quadVAO);

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int current_x = 0;
    for (const char c : text)
    {
        auto it = m_charToRectMap.find(c);
        if (it == m_charToRectMap.end()) continue;

        const auto& char_rect = it->second;

        const Math::ivec2 char_size = {
            char_rect.top_right.x - char_rect.bottom_left.x,
            char_rect.top_right.y - char_rect.bottom_left.y
        };

        Math::Vec2 charCenter = {
            static_cast<float>(current_x + char_size.x / 2.0f),
            static_cast<float>(char_size.y / 2.0f)
        };
        Math::Matrix model = Math::Matrix::CreateTranslation(charCenter) * Math::Matrix::CreateScale({ static_cast<float>(char_size.x), static_cast<float>(char_size.y) });
        atlasShader.setMat4("model", model);

        float uv_x = static_cast<float>(char_rect.bottom_left.x) / m_atlasWidth;
        float uv_w = static_cast<float>(char_size.x) / m_atlasWidth;
        float uv_h = static_cast<float>(char_size.y) / m_atlasHeight;
        float uv_y = 1.0f - (static_cast<float>(char_rect.top_right.y) / m_atlasHeight);

        const float pixel_epsilon_x = 0.5f / m_atlasWidth;
        uv_x += pixel_epsilon_x;
        uv_w -= 2.0f * pixel_epsilon_x;

        const float one_pixel_y = 1.0f / m_atlasHeight;
        uv_y += one_pixel_y;
        uv_h -= 2.0f * one_pixel_y;

        atlasShader.setVec4("spriteRect", uv_x, uv_y, uv_w, uv_h);

        GL::DrawArrays(GL_TRIANGLES, 0, 6);

        current_x += char_size.x;
    }

    GL::BindFramebuffer(GL_FRAMEBUFFER, 0);

    return { newTexID, textSize.x, textSize.y };
}

void Font::DrawBakedText(Shader& textureShader, const CachedTextureInfo& textureInfo, Math::Vec2 position, float newHeight)
{
    if (textureInfo.textureID == 0 || textureInfo.height == 0)
    {
        return;
    }

    textureShader.use();

    float scale = newHeight / m_fontHeight;
    float renderWidth = textureInfo.width * scale;
    float renderHeight = textureInfo.height * scale;

    Math::Vec2 center = {
        position.x + renderWidth / 2.0f,
        position.y + renderHeight / 2.0f
    };

    Math::Matrix model = Math::Matrix::CreateTranslation(center) *
        Math::Matrix::CreateScale({ renderWidth, -renderHeight });
    textureShader.setMat4("model", model);

    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    textureShader.setBool("flipX", false);

    textureShader.setFloat("alpha", 1.0f);

    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindTexture(GL_TEXTURE_2D, textureInfo.textureID);
    GL::BindVertexArray(m_quadVAO);

    GL::DrawArrays(GL_TRIANGLES, 0, 6);

    GL::BindVertexArray(0);
}

Math::ivec2 Font::measureText(const std::string& text) const
{
    if (text.empty()) return { 0, m_fontHeight };

    int total_width = 0;
    for (const char c : text)
    {
        auto it = m_charToRectMap.find(c);
        if (it != m_charToRectMap.end())
        {
            total_width += (it->second.top_right.x - it->second.bottom_left.x);
        }
    }
    return { total_width, m_fontHeight };
}