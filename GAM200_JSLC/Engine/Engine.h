#pragma once
#include <string>
#include "../Game/Player.h"

// 전방 선언
class Shader;
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
    Shader* p_shader = nullptr;
    Shader* p_solid_color_shader = nullptr;

    unsigned int groundVAO = 0;
    unsigned int groundVBO = 0;

    Player player; 

    static double deltaTime;
    double lastFrameTime = 0.0;
};