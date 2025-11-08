// Camera.cpp

#include "Camera.hpp"
#include <algorithm>

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
    // 부드럽게 타겟을 따라가기
    Math::Vec2 desired = targetPosition;

    // 경계가 설정되어 있으면 카메라 위치 제한
    if (m_hasBounds)
    {
        float halfWidth = m_viewWidth / 2.0f;
        float halfHeight = m_viewHeight / 2.0f;

        desired.x = std::max(m_minBounds.x + halfWidth, std::min(desired.x, m_maxBounds.x - halfWidth));
        desired.y = std::max(m_minBounds.y + halfHeight, std::min(desired.y, m_maxBounds.y - halfHeight));
    }

    // 선형 보간으로 부드러운 이동
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

Math::Matrix Camera::GetViewMatrix() const
{
    // 카메라 중심을 화면 중앙으로
    float offsetX = m_viewWidth / 2.0f - m_position.x;
    float offsetY = m_viewHeight / 2.0f - m_position.y;

    return Math::Matrix::CreateTranslation({ offsetX, offsetY });
}