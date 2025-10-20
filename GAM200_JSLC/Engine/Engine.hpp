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

    // 키보드 입력을 처리할 콜백 함수 (static으로 선언해야 함)
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    GLFWwindow* m_window = nullptr;
    int m_width = 1980;
    int m_height = 1080;
    std::unique_ptr<GameStateManager> m_gameStateManager = nullptr;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;

    // --- 전체화면 상태 저장을 위한 변수들 ---
    bool m_isFullscreen = false;
    // 창 모드일 때의 위치와 크기를 저장해 둘 변수
    int m_windowedX = 100;
    int m_windowedY = 100;
    int m_windowedWidth = 1980;
    int m_windowedHeight = 1080;
};