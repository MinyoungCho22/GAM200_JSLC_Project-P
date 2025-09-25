#pragma once
#include <string>

// ���� ����
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

    // ������ �� �ð� ���� (��Ÿ Ÿ��)
    static double deltaTime;
    double lastFrameTime = 0.0;
};