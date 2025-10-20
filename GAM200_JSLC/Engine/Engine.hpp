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

    void ToggleFullscreen();

private:
    void Update();

    // Ű���� �Է��� ó���� �ݹ� �Լ� (static���� �����ؾ� ��)
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    GLFWwindow* m_window = nullptr;
    int m_width = 1980;
    int m_height = 1080;
    std::unique_ptr<GameStateManager> m_gameStateManager = nullptr;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;

    // --- ��üȭ�� ���� ������ ���� ������ ---
    bool m_isFullscreen = false;
    // â ����� ���� ��ġ�� ũ�⸦ ������ �� ����
    int m_windowedX = 100;
    int m_windowedY = 100;
    int m_windowedWidth = 1980;
    int m_windowedHeight = 1080;
};