#include "Camera.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <random>   

Camera::Camera(Math::Rect player_zone)
    : m_player_zone(player_zone)
{
}

void Camera::SetPosition(Math::Vec2 new_position)
{
    m_position = new_position;
}

const Math::Vec2& Camera::GetPosition() const
{
    return m_position;
}

void Camera::SetLimit(Math::Rect new_limit)
{
    m_limit = new_limit;
}

const Math::Rect& Camera::GetLimit() const
{
    return m_limit;
}

void Camera::SetZone(Math::Rect new_zone)
{
    m_player_zone = new_zone;
}

Math::Matrix Camera::GetViewMatrix() const
{
    Math::Vec2 final_position = m_position;

    if (m_shake_timer > 0.0f)
    {
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

        final_position.x += dis(gen) * m_shake_magnitude;
        final_position.y += dis(gen) * m_shake_magnitude;
    }

    return Math::Matrix::CreateTranslation({ -final_position.x, -final_position.y });
}

void Camera::Shake(float duration, float magnitude)
{
    if (m_shake_timer <= 0.0f)
    {
        m_shake_timer = duration;
        m_shake_magnitude = magnitude;
        Logger::Instance().Log(Logger::Severity::Event, "Camera shake initiated.");
    }
}

void Camera::CenterOn(Math::Vec2 target_position)
{
    m_position = target_position;

    m_position.x = std::max(m_limit.bottom_left.x, m_position.x);
    m_position.x = std::min(m_limit.top_right.x, m_position.x);
    m_position.y = std::max(m_limit.bottom_left.y, m_position.y);
    m_position.y = std::min(m_limit.top_right.y, m_position.y);
}

void Camera::Update(const Math::Vec2& player_position, double dt)
{
    if (m_shake_timer > 0.0f)
    {
        m_shake_timer -= static_cast<float>(dt);
        if (m_shake_timer <= 0.0f)
        {
            m_shake_magnitude = 0.0f;
        }
    }

    Math::Vec2 target_position = m_position;

    // 1. 플레이어가 존을 벗어났는지 확인하고 목표 위치 계산
    float zone_right_edge = m_position.x + m_player_zone.top_right.x;
    if (player_position.x > zone_right_edge)
    {
        target_position.x = player_position.x - m_player_zone.top_right.x;
    }

    float zone_left_edge = m_position.x + m_player_zone.bottom_left.x;
    if (player_position.x < zone_left_edge)
    {
        target_position.x = player_position.x - m_player_zone.bottom_left.x;
    }

    float zone_top_edge = m_position.y + m_player_zone.top_right.y;
    if (player_position.y > zone_top_edge)
    {
        target_position.y = player_position.y - m_player_zone.top_right.y;
    }

    float zone_bottom_edge = m_position.y + m_player_zone.bottom_left.y;
    if (player_position.y < zone_bottom_edge)
    {
        target_position.y = player_position.y - m_player_zone.bottom_left.y;
    }

    // 2. 목표 위치를 월드 경계(limit) 안에 있도록 제한
    bool is_clamped = false;
    float original_target_x = target_position.x;
    float original_target_y = target_position.y;

    target_position.x = std::max(m_limit.bottom_left.x, target_position.x);
    target_position.x = std::min(m_limit.top_right.x, target_position.x);
    target_position.y = std::max(m_limit.bottom_left.y, target_position.y);
    target_position.y = std::min(m_limit.top_right.y, target_position.y);

    if (target_position.x != original_target_x || target_position.y != original_target_y)
    {
        is_clamped = true;
    }
    if (is_clamped)
    {
        Logger::Instance().Log(Logger::Severity::Debug, "Camera position clamped to world limits.");
    }

 
    // Math::Vec2 Math::Lerp(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
    if (m_smoothing > 0.0f)
    {
        float lerp_factor = 1.0f - std::exp(-m_smoothing * static_cast<float>(dt));
        m_position = Math::Lerp(m_position, target_position, lerp_factor);
    }
    else
    {
        m_position = target_position;
    }
}