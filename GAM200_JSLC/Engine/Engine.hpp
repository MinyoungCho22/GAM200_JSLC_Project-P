#pragma once
#include <string>
#include <memory>
#include "../Game/Player.hpp"

class Shader;
struct GLFWwindow;

class Engine
{
public:
    // [추가] 엔진의 상태를 정의합니다.
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

    // [추가] 스플래시 스크린을 그리는 전용 함수
    void DrawSplashScreen() const;

    // --- 멤버 변수 ---
    GLFWwindow* m_window = nullptr;
    std::unique_ptr<Shader> m_shader;
    std::unique_ptr<Shader> m_solid_color_shader;

    int m_width = 1280;
    int m_height = 720;

    // 게임플레이 리소스
    unsigned int m_groundVAO = 0;
    unsigned int m_groundVBO = 0;
    Player m_player;

    // [추가] 스플래시 스크린 리소스
    unsigned int m_splashVAO = 0;
    unsigned int m_splashVBO = 0;
    unsigned int m_splashTextureID = 0;

    // 상태 관리 변수
    State m_currentState = State::Splash;
    double m_splashTimer = 3.0; // 3초

    // 시간 관리 변수
    double m_deltaTime = 0.0;
    double m_lastFrameTime = 0.0;
};