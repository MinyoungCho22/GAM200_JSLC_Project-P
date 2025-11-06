#pragma once
#include "../Engine/Matrix.hpp"
#include "../Engine/Vec2.hpp" // Vec2를 사용하기 위해 추가

// Shader 클래스의 전방 선언
class Shader;

class Background
{
public:
    Background() = default;

    void Initialize(const char* texturePath);
    void Shutdown();
    void Draw(Shader& shader, const Math::Matrix& model);

    // ✅ [추가] 원본 이미지의 크기를 반환하는 함수
    Math::Vec2 GetImageSize() const { return { (float)m_imageWidth, (float)m_imageHeight }; }

private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int m_textureID = 0;

    // ✅ [추가] 원본 이미지의 너비와 높이
    int m_imageWidth = 0;
    int m_imageHeight = 0;
};