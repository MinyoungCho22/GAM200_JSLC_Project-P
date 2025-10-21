#pragma once
#include "Vec2.hpp"

class Shader;

class DebugRenderer
{
public:
    void Initialize();
    void Shutdown() const;

  
    void DrawCircle(Shader& shader, Math::Vec2 center, float radius, Math::Vec2 color) const;

    
    void DrawBox(Shader& shader, Math::Vec2 pos, Math::Vec2 size, Math::Vec2 color) const;

private:
    unsigned int circleVAO = 0;
    unsigned int boxVAO = 0;
    unsigned int VBO = 0, EBO = 0;
    static constexpr int CIRCLE_SEGMENTS = 24;
};