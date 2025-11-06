#pragma once
#include <string>
#include <memory>
#include "Input.hpp" 

struct GLFWwindow;
class GameStateManager;

// ✅ [추가] 게임의 기준이 되는 가상 해상도를 정의합니다.
constexpr int VIRTUAL_WIDTH = 1920;
constexpr int VIRTUAL_HEIGHT = 1080;

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

    // ✅ GetWidth/Height가 항상 가상 해상도를 반환하도록 수정
    int GetWidth() const { return VIRTUAL_WIDTH; }
    int GetHeight() const { return VIRTUAL_HEIGHT; }

    Input::Input& GetInput() const { return *m_input; }
    void RequestShutdown();

    void ToggleFullscreen();

private:
    void Update();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);

    // ✅ OnFramebufferResize의 시그니처가 m_width/m_height를 사용하지 않으므로 변경
    void OnFramebufferResize(int newScreenWidth, int newScreenHeight);

    GLFWwindow* m_window = nullptr;
    // ✅ m_width/m_height는 이제 가상 해상도를 의미하므로 const로 변경
    const int m_width = VIRTUAL_WIDTH;
    const int m_height = VIRTUAL_HEIGHT;
    std::unique_ptr<GameStateManager> m_gameStateManager = nullptr;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;

    std::unique_ptr<Input::Input> m_input;

    bool m_isFullscreen = false;
    int m_windowedX = 100;
    int m_windowedY = 100;
    int m_windowedWidth = VIRTUAL_WIDTH;  // ✅ 1920
    int m_windowedHeight = VIRTUAL_HEIGHT; // ✅ 1080
};