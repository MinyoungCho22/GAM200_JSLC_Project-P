#pragma once
#include "../Engine/Matrix.hpp"

class Shader;

class Background
{
public:
    Background() = default;

    void Initialize(const char* texturePath);
    void Shutdown();
    void Draw(Shader& shader, const Math::Matrix& model);

private:
    unsigned int m_textureID = 0;
};