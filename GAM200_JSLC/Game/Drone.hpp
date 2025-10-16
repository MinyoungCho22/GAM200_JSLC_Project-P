#pragma once
#include "../Engine/Vec2.hpp"

class Shader;

class Drone
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath);
    void Update(double dt);
    void Draw(const Shader& shader) const;
    void Shutdown();
    void TakeHit(); // [�߰�]

    Math::Vec2 GetPosition() const { return m_position; }
    Math::Vec2 GetSize() const { return m_size; }

private:
    Math::Vec2 m_position;
    Math::Vec2 m_velocity;
    Math::Vec2 m_direction;
    Math::Vec2 m_size;

    // [�߰�] �ǰ� ���� ���� ����
    bool m_isHit = false;
    float m_hitTimer = 0.0f;
    const float m_hitDuration = 0.5f; // 0.5�ʰ� ��鸲

    float m_speed = 200.0f;
    float m_moveTimer = 0.0f;

    unsigned int VAO = 0, VBO = 0, textureID = 0;
};