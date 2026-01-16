// Camera.hpp

#pragma once
#include "Vec2.hpp"
#include "Matrix.hpp"

class Camera
{
public:
    Camera();

    void Initialize(Math::Vec2 position, float viewWidth, float viewHeight);
    void Update(Math::Vec2 targetPosition, float smoothSpeed = 0.1f);
    void SetPosition(Math::Vec2 position);
    void SetBounds(Math::Vec2 minBounds, Math::Vec2 maxBounds);

    Math::Matrix GetViewMatrix() const;
    Math::Vec2 GetPosition() const { return m_position; }

private:
    Math::Vec2 m_position;
    float m_viewWidth;
    float m_viewHeight;
    Math::Vec2 m_minBounds;
    Math::Vec2 m_maxBounds;
    bool m_hasBounds;
};