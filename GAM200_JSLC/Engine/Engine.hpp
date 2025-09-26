#pragma once
#include <string>
#include <memory>
#include "../Game/Player.hpp"

class Shader;
struct GLFWwindow;

class Engine
{
public:
    Engine();
    ~Engine();

    bool Initialize(const std::string& windowTitle);
    void GameLoop();
    void Shutdown();

    double GetDeltaTime() const { return m_deltaTime; }

private:
    void Update();
    void Draw() const;

    GLFWwindow* m_window = nullptr;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Shader> m_solid_color_shader;

    int m_width = 1280;
    int m_height = 720;

    unsigned int m_groundVAO = 0;
    unsigned int m_groundVBO = 0;

    Player m_player;

    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;
};