#pragma once
#include <string>

// 전방 선언
struct GLFWwindow;

class Engine
{
public:
    void Initialize(const std::string& windowTitle);
    void GameLoop();
    void Shutdown();

    static double GetDeltaTime() { return deltaTime; }

private:
    void Update();
    void Draw() const;

    GLFWwindow* p_window = nullptr;

    // 프레임 간 시간 간격 (델타 타임)
    static double deltaTime;
    double lastFrameTime = 0.0;
};