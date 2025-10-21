#pragma once
#include "Vec2.hpp"
#include "Matrix.hpp"
#include "Rect.hpp"

class Camera
{
public:
    Camera(Math::Rect player_zone);

    // ī�޶��� ��ġ�� ��� ����
    void SetPosition(Math::Vec2 new_position);
    // ī�޶��� ���� ��ġ�� ��ȯ
    [[nodiscard]] const Math::Vec2& GetPosition() const;

    // ī�޶� ������ �� �ִ� ������ ��踦 ����
    void SetLimit(Math::Rect new_limit);
    // ī�޶��� ���� ��踦 ��ȯ
    [[nodiscard]] const Math::Rect& GetLimit() const;

    // �÷��̾ �����Ӱ� ������ �� �ִ� ȭ�� �� ������ ����
    void SetZone(Math::Rect new_zone);

    // �������� ����� ��(view) ����� ����Ͽ� ��ȯ
    [[nodiscard]] Math::Matrix GetViewMatrix() const;

    // ������ �ð��� ������ ī�޶� ����
    void Shake(float duration, float magnitude);

    // ������ ��ġ�� ī�޶� ��� �̵� (��� ���� ����).
    void CenterOn(Math::Vec2 target_position);

    // �� ������ ȣ��Ǿ� �÷��̾� ��ġ�� ���� ī�޶� ������Ʈ
    void Update(const Math::Vec2& player_position, double dt);

private:
    Math::Rect m_limit;
    Math::Vec2 m_position{ 0.0f, 0.0f };
    Math::Rect m_player_zone;

    // �ε巯�� �̵��� ���� ����
    float m_smoothing = 15.0f;

    // ī�޶� ����ũ�� ���� ����
    float m_shake_timer = 0.0f;
    float m_shake_magnitude = 0.0f;
};