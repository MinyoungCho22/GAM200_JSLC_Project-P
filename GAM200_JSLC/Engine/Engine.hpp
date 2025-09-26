#pragma once
#include <string>
#include <memory>
#include "../Game/Player.hpp"

class Shader;
struct GLFWwindow;

class Engine
{
public:
    enum class State { Splash, Gameplay };

    Engine();
    ~Engine();

    bool Initialize(const std::string& windowTitle);
    void GameLoop();
    void Shutdown();

    double GetDeltaTime() const { return m_deltaTime; }

private:
    void Update();
    void Draw() const;
    void DrawSplashScreen() const;

    GLFWwindow* m_window = nullptr;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Shader> m_solid_color_shader;

    int m_width = 1280;
    int m_height = 720;

    // 게임플레이 리소스
    unsigned int m_groundVAO = 0;
    unsigned int m_groundVBO = 0;
    Player m_player;

    // 스플래시 스크린 리소스
    unsigned int m_splashVAO = 0;
    unsigned int m_splashVBO = 0;
    unsigned int m_splashTextureID = 0;
    int m_splashImageWidth = 0;
    int m_splashImageHeight = 0;

    // 상태 관리 변수
    State m_currentState = State::Splash;
    double m_splashTimer = 3.0;

    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;
};