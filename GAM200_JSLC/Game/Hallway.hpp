// Hallway.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include <memory>

class Background;
class Shader;

class Hallway
{
public:

    static constexpr float WIDTH = 5940.0f;
    static constexpr float HEIGHT = 1080.0f;

    void Initialize();
    void Update(double dt);
    void Draw(Shader& shader);
    void Shutdown();

private:
    std::unique_ptr<Background> m_background;
    Math::Vec2 m_position;
    Math::Vec2 m_size;
};