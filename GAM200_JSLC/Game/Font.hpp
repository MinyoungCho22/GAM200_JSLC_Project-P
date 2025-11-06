#pragma once
#include "../Engine/Vec2.hpp"
#include <string>
#include <map>

class Shader;
class Matrix;

class Font
{
public:
    Font() = default;

    // 폰트 텍스처를 로드하고, 텍스처가 가로/세로 몇 개의 문자로 이루어져 있는지 설정합니다.
    void Initialize(const char* texturePath, int columns, int rows);
    void Shutdown();

    // 지정된 셰이더를 사용해 텍스트를 그립니다.
    void DrawText(Shader& shader, const std::string& text, Math::Vec2 position, float size);

private:
    unsigned int m_textureID = 0;
    unsigned int VAO = 0;
    unsigned int VBO = 0;

    int m_texWidth = 0;
    int m_texHeight = 0;
    int m_numCols = 1;
    int m_numRows = 1;
    float m_charWidth = 1.0f;  // 0.0 ~ 1.0 UV Rati
    float m_charHeight = 1.0f; // 0.0 ~ 1.0 UV Ratio
};