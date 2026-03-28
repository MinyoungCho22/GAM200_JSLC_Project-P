// Camera.cpp

#include "Camera.hpp"
#include <algorithm>
#include <cmath>

Camera::Camera()
    : m_position(0.0f, 0.0f)
    , m_viewWidth(1920.0f)
    , m_viewHeight(1080.0f)
    , m_minBounds(0.0f, 0.0f)
    , m_maxBounds(1920.0f, 1080.0f)
    , m_hasBounds(false)
{
}

void Camera::Initialize(Math::Vec2 position, float viewWidth, float viewHeight)
{
    m_position = position;
    m_viewWidth = viewWidth;
    m_viewHeight = viewHeight;
}

void Camera::Update(Math::Vec2 targetPosition, float smoothSpeed)
{
    // Skip regular update while camera animation is active.
    if (m_isAnimating)
    {
        return;
    }

    Math::Vec2 desired = targetPosition;

    if (m_hasBounds)
    {
        float halfWidth = m_viewWidth / 2.0f;
        float halfHeight = m_viewHeight / 2.0f;

        desired.x = std::max(m_minBounds.x + halfWidth, std::min(desired.x, m_maxBounds.x - halfWidth));
        desired.y = std::max(m_minBounds.y + halfHeight, std::min(desired.y, m_maxBounds.y - halfHeight));
    }

    m_position.x += (desired.x - m_position.x) * smoothSpeed;
    m_position.y += (desired.y - m_position.y) * smoothSpeed;
}

void Camera::SetPosition(Math::Vec2 position)
{
    m_position = position;
}

void Camera::SetBounds(Math::Vec2 minBounds, Math::Vec2 maxBounds)
{
    m_minBounds = minBounds;
    m_maxBounds = maxBounds;
    m_hasBounds = true;
}

void Camera::StartAnimation(Math::Vec2 startPos, Math::Vec2 endPos, float duration)
{
    m_isAnimating = true;
    m_animStartPos = startPos;
    m_animEndPos = endPos;
    m_animDuration = duration;
    m_animElapsed = 0.0f;
    m_position = startPos;
}

void Camera::UpdateAnimation(float dt)
{
    if (!m_isAnimating)
    {
        return;
    }

    m_animElapsed += dt;

    if (m_animElapsed >= m_animDuration)
    {
        m_position = m_animEndPos;
        m_isAnimating = false;
        return;
    }

    // Ease-in-out interpolation (smooth acceleration/deceleration).
    float t = m_animElapsed / m_animDuration;
    float easedT = t < 0.5f 
        ? 2.0f * t * t 
        : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;

    m_position.x = m_animStartPos.x + (m_animEndPos.x - m_animStartPos.x) * easedT;
    m_position.y = m_animStartPos.y + (m_animEndPos.y - m_animStartPos.y) * easedT;
}

void Camera::StopAnimation()
{
    m_isAnimating = false;
}

Math::Matrix Camera::GetViewMatrix() const
{
    float offsetX = m_viewWidth / 2.0f - m_position.x;
    float offsetY = m_viewHeight / 2.0f - m_position.y;

    // Snap scroll to integer pixels in framebuffer space. Sub-pixel camera
    // translation lets adjacent quads land on fractional device coordinates;
    // the rasterizer can then leave 1px gaps that show the clear color as
    // flickering black/dark lines (more noticeable after upscaling to 1080p+).
    offsetX = std::round(offsetX);
    offsetY = std::round(offsetY);

    return Math::Matrix::CreateTranslation({ offsetX, offsetY });
}