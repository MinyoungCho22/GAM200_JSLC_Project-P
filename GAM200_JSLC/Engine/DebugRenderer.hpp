// DebugRenderer.hpp

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
    void DrawBox(Shader& shader, Math::Vec2 pos, Math::Vec2 size, float r, float g, float b) const;
    /// thickness: 월드 단위 선 폭 (기본 1 — 얇은 디버그 라인)
    void DrawLine(const Shader& shader, Math::Vec2 start, Math::Vec2 end, float r, float g, float b,
                  float thickness = 0.35f) const;

private:
    unsigned int circleVAO = 0;
    unsigned int boxVAO = 0;
    unsigned int lineVAO = 0;
    unsigned int lineVBO = 0;
    unsigned int lineEBO = 0;
    unsigned int VBO = 0, EBO = 0;
    // Enough segments so attack-range / debug circles read as smooth curves, not obvious polygons.
    static constexpr int CIRCLE_SEGMENTS = 96;
};