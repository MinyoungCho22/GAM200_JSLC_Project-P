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
    Math::Vec2 GetPosition() const { return m_position; }

private:
    Math::Vec2 m_position;
    Math::Vec2 m_velocity;
    Math::Vec2 m_direction;
    Math::Vec2 m_size;

    float m_speed = 200.0f;
    float m_moveTimer = 0.0f;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
};