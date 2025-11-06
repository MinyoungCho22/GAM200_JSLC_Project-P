#pragma once
#include "../Engine/Matrix.hpp"

// Shader 클래스의 전방 선언
class Shader;

class Background
{
public:
    Background() = default;

    // 텍스처를 로드하고 그리기 위한 VAO/VBO를 설정합니다.
    void Initialize(const char* texturePath);

    // VAO/VBO/텍스처 리소스를 해제합니다.
    void Shutdown();

    // 배경을 그립니다. 셰이더와 모델 행렬을 받습니다.
    void Draw(Shader& shader, const Math::Matrix& model);

private:
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int m_textureID = 0;
};