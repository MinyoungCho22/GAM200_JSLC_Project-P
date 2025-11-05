#pragma once
#include <string>
#include <memory>
#include "Input.hpp" // Input 헤더 포함

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
    void RequestShutdown();
    double GetDeltaTime() const { return m_deltaTime; }
    GLFWwindow* GetWindow() const { return m_window; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    Input::Input& GetInput() const { return *m_input; }

    void ToggleFullscreen();

private:
    void Update();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    GLFWwindow* m_window = nullptr;
    int m_width = 1980;
    int m_height = 1080;
    std::unique_ptr<GameStateManager> m_gameStateManager = nullptr;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;

    std::unique_ptr<Input::Input> m_input; // ✅ Input::Input 타입으로 수정

    bool m_isFullscreen = false;
    int m_windowedX = 100;
    int m_windowedY = 100;
    int m_windowedWidth = 1980;
    int m_windowedHeight = 1080;
};