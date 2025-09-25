#include "Engine.h"
#include "../OpenGL/Shader.h"
#include "../Engine/Matrix.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

// 전역 Player 객체 삭제
double Engine::deltaTime = 0.0;

void Engine::Initialize(const std::string& windowTitle)
{
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    p_window = glfwCreateWindow(1280, 720, windowTitle.c_str(), NULL, NULL);
    if (!p_window) { std::cerr << "Failed to create GLFW window" << std::endl; glfwTerminate(); return; }
    glfwMakeContextCurrent(p_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; return; }

    glViewport(0, 0, 1280, 720);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    p_shader = new Shader("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    p_solid_color_shader = new Shader("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");

    float line_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };

    glGenVertexArrays(1, &groundVAO);
    glGenBuffers(1, &groundVBO);
    glBindVertexArray(groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 멤버 변수가 된 player 객체 초기화
    player.Init({ 100.0f, 300.0f }, "Asset/player.png");
}

void Engine::Draw() const
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Matrix projection = Matrix::CreateOrtho(0.0f, 1280.0f, 0.0f, 720.0f, -1.0f, 1.0f);

    // --- 플레이어 그리기 ---
    p_shader->use();
    p_shader->setMat4("projection", projection);
    player.Draw(*p_shader);

    // --- 추가: 바닥 선 그리기 ---
    p_solid_color_shader->use();
    p_solid_color_shader->setMat4("projection", projection);

    Vec2 groundPosition = { 1280.0f / 2.0f, 100.0f };
    Vec2 groundSize = { 1280.0f, 2.0f };
    Matrix groundModel = Matrix::CreateTranslation(groundPosition) * Matrix::CreateScale(groundSize);

    p_solid_color_shader->setMat4("model", groundModel);

    GLint colorLocation = glGetUniformLocation(p_solid_color_shader->ID, "objectColor");
    glUniform3f(colorLocation, 0.8f, 0.8f, 0.8f); // 밝은 회색

    glBindVertexArray(groundVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Engine::Shutdown()
{
    // --- 수정: 종료 순서 변경 ---
    // OpenGL 리소스 해제를 먼저 수행
    player.Shutdown();
    delete p_shader;
    delete p_solid_color_shader;
    glDeleteVertexArrays(1, &groundVAO);
    glDeleteBuffers(1, &groundVBO);

    // 모든 OpenGL 리소스 해제 후 GLFW 종료
    glfwTerminate();
}

void Engine::GameLoop()
{
    while (!glfwWindowShouldClose(p_window))
    {
        double currentFrameTime = glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        glfwPollEvents();
        Update();
        Draw();
        glfwSwapBuffers(p_window);
    }
}

void Engine::Update()
{
    player.Update(deltaTime, p_window);
}