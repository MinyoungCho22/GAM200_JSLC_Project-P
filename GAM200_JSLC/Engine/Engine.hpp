//Engine.hpp

#pragma once
#include <string>
#include <memory>
#include "Input.hpp"

struct GLFWwindow;
class GameStateManager;
class Shader; 


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
    void RequestShutdown();
    double GetDeltaTime() const { return m_deltaTime; }
    GLFWwindow* GetWindow() const { return m_window; }

 
    int GetWidth() const { return VIRTUAL_WIDTH; }
    int GetHeight() const { return VIRTUAL_HEIGHT; }

    Input::Input& GetInput() const { return *m_input; }

 
    Shader& GetTextureShader() const { return *m_textureShader; }

    void ToggleFullscreen();

private:
    void Update();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    void OnFramebufferResize(int newScreenWidth, int newScreenHeight);

    GLFWwindow* m_window = nullptr;
   
    const int m_width = VIRTUAL_WIDTH;
    const int m_height = VIRTUAL_HEIGHT;
    std::unique_ptr<GameStateManager> m_gameStateManager = nullptr;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;

    std::unique_ptr<Input::Input> m_input;

    // 공용 텍스처 셰이더 (simple.vert/frag)
    std::unique_ptr<Shader> m_textureShader;

    bool m_isFullscreen = false;
    int m_windowedX = 100;
    int m_windowedY = 100;
    
    int m_windowedWidth = VIRTUAL_WIDTH;
    int m_windowedHeight = VIRTUAL_HEIGHT;
};