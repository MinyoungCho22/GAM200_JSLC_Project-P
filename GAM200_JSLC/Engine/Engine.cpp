#include "Engine.h"
#include "../Game/Player.h" // 게임 오브젝트 포함

// OpenGL 및 창 관련 라이브러리
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

// 정적 멤버 변수 초기화
double Engine::deltaTime = 0.0;

// 임시 플레이어 객체
Player player;

void Engine::Initialize(const std::string& windowTitle)
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // [수정] 코어 프로파일 -> 호환 프로파일로 변경
    // 이렇게 하면 glBegin/glEnd 같은 구식 함수들이 임시로 동작합니다.
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    p_window = glfwCreateWindow(1280, 720, windowTitle.c_str(), NULL, NULL);
    if (!p_window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return;
    }
    glfwMakeContextCurrent(p_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return;
    }

    glViewport(0, 0, 1280, 720);

    // 플레이어 초기화
    player.Init({ 100.0, 300.0 });
}

void Engine::GameLoop()
{
    while (!glfwWindowShouldClose(p_window))
    {
        // 1. 델타 타임 계산
        double currentFrameTime = glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // 2. 입력 처리
        glfwPollEvents();
        if (glfwGetKey(p_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(p_window, true);

        // 3. 게임 로직 업데이트
        Update();

        // 4. 렌더링
        Draw();

        // 5. 버퍼 스왑
        glfwSwapBuffers(p_window);
    }
}

void Engine::Shutdown()
{
    glfwTerminate();
}

void Engine::Update()
{
    // 플레이어 업데이트 (물리 계산 포함)
    player.Update(deltaTime, p_window);
}

void Engine::Draw() const
{
    // 화면을 검은색으로 클리어
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 2D 렌더링을 위한 좌표계 설정
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1280.0, 0.0, 720.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // 플레이어 그리기
    player.Draw();
}