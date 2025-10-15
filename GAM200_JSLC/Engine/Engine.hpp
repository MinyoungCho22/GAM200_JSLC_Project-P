#pragma once
#include <string>
#include <memory>

struct GLFWwindow;
class GameStateManager;

class Engine
{
public:
    Engine();
    ~Engine();
    bool Initialize(const std::string& windowTitle);
    void GameLoop();
    void Shutdown();
    double GetDeltaTime() const { return m_deltaTime; }
    GLFWwindow* GetWindow() const { return m_window; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    void Update();

    GLFWwindow* m_window = nullptr;
    int m_width, m_height;
    std::unique_ptr<GameStateManager> m_gameStateManager;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;
};