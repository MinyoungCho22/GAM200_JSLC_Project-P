#pragma once

#include "../Engine/Vec2.hpp"
#include "../Engine/Rect.hpp"
#include <string>
#include <map>
#include <unordered_map>
#include <memory> 

#include "../OpenGL/GLWrapper.hpp" 

class Shader;

struct CachedTextureInfo
{
    unsigned int textureID = 0;
    int width = 0;
    int height = 0;
    std::string text;
};

class Font
{
public:
    Font() = default;
    ~Font() { Shutdown(); }

    void Initialize(const char* fontAtlasPath);
    void Shutdown();

    CachedTextureInfo PrintToTexture(Shader& atlasShader, const std::string& text);
    void DrawBakedText(Shader& textureShader, const CachedTextureInfo& textureInfo, Math::Vec2 position, float newHeight);
    int m_fontHeight = 0;
private:
    unsigned int GetPixel(const unsigned char* data, int x, int y, int width, int channels) const;
    CachedTextureInfo BakeTextToTexture(Shader& atlasShader, const std::string& text);
    Math::ivec2 measureText(const std::string& text) const;

private:
    unsigned int m_atlasTextureID = 0;
    int m_atlasWidth = 0;
    int m_atlasHeight = 0;

    const std::string m_charSequence = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

    
    std::map<char, Math::IRect> m_charToRectMap;

    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;
    unsigned int m_fboID = 0;

    std::unordered_map<std::string, CachedTextureInfo> m_textCache;
};