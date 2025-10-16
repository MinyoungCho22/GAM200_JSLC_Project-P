#pragma once
<<<<<<< HEAD
#include "../Engine/Vec2.hpp" 
=======
#include "../Engine/Vec2.hpp"

>>>>>>> 96909bfd1549a09b7e09b0ce175d60a91d54ec7f
class Shader;

class Drone
{
public:
<<<<<<< HEAD
	void Init(Math::Vec2 drone_position, const char* texturePath);
	void Update(double dt);
	void Draw(const Shader& shader) const;
	void Shutdown();
Math::Vec2 GetPosition() const { return drone_position; } private:
	Math::Vec2 drone_position{ 0.0f, 0.0f };
	Math::Vec2 drone_velocity{ 0.0f, 0.0f };
	Math::Vec2 drone_direction{ 0.0f, 0.0f };
	Math::Vec2 drone_size{ 0.0f, 0.0f };

	float drone_speed = 200.0f;
	float move_timer = 0.0f;
	unsigned int VAO = 0;
	unsigned int VBO = 0; unsigned int textureID = 0;
=======
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
>>>>>>> 96909bfd1549a09b7e09b0ce175d60a91d54ec7f
};