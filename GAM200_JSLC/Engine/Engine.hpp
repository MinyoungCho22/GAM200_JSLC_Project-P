//Engine.hpp

#pragma once
#include <string>
#include <memory>
#include "ControlBindings.hpp"
#include "Input.hpp"
#include "Vec2.hpp"
#include "../OpenGL/PostProcessManager.h"
#include "GameStateManager.hpp"

struct GLFWwindow;
class Shader;
class ImguiManager;
class DroneConfigManager;
class RobotConfigManager;

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

    ControlBindings& GetControlBindings() const { return *m_controlBindings; }

    Shader& GetTextureShader() const { return *m_textureShader; }

    ImguiManager* GetImguiManager() const { return m_imguiManager.get(); }
    ImguiManager* GetImguiManager() { return m_imguiManager.get(); }
    std::shared_ptr<DroneConfigManager> GetDroneConfigManager() { return m_droneConfigManager; }
    std::shared_ptr<RobotConfigManager> GetRobotConfigManager() { return m_robotConfigManager; }

    void ToggleFullscreen();
    void SetFullscreen(bool enabled);
    bool IsFullscreen() const { return m_isFullscreen; }

    Math::ivec2 GetRecommendedResolution();

    void SetResolution(int width, int height);

    PostProcessManager& GetPostProcess() { return *m_postProcess; }

    void SetVSync(bool enabled);
    void SetFpsCap(int cap);
    bool IsVSyncEnabled() const { return m_vsyncEnabled; }
    int GetFpsCap() const { return m_fpsCap; }

private:
    void Update();

    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowFocusCallback(GLFWwindow* window, int focused);
    void OnFramebufferResize(int newScreenWidth, int newScreenHeight);
    void SyncPostProcessDisplaySize();
    void ApplyCustomCursorHidden();

    GLFWwindow* m_window = nullptr;

    const int m_width = VIRTUAL_WIDTH;
    const int m_height = VIRTUAL_HEIGHT;
    std::unique_ptr<GameStateManager> m_gameStateManager = nullptr;
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;

    std::unique_ptr<Input::Input> m_input;
    std::unique_ptr<ControlBindings> m_controlBindings;

    std::unique_ptr<Shader> m_textureShader;

    std::unique_ptr<ImguiManager> m_imguiManager;
    std::shared_ptr<DroneConfigManager> m_droneConfigManager;
    std::shared_ptr<RobotConfigManager> m_robotConfigManager;

    bool m_vsyncEnabled = false;
    int  m_fpsCap = 0;

    bool m_isFullscreen = false;
    int m_windowedX = 100;
    int m_windowedY = 100;

    int m_windowedWidth = VIRTUAL_WIDTH;
    int m_windowedHeight = VIRTUAL_HEIGHT;

    std::unique_ptr<PostProcessManager> m_postProcess;
};