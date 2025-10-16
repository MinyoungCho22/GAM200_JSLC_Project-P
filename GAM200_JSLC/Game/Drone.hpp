#pragma once
#include "../Engine/Vec2.hpp" 
class Shader;

class Drone
{
public:
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
};