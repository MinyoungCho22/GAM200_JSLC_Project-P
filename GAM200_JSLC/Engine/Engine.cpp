#include "Engine.h"
#include "../Game/Player.h" // ���� ������Ʈ ����

// OpenGL �� â ���� ���̺귯��
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

// ���� ��� ���� �ʱ�ȭ
double Engine::deltaTime = 0.0;

// �ӽ� �÷��̾� ��ü
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

    // [����] �ھ� �������� -> ȣȯ �������Ϸ� ����
    // �̷��� �ϸ� glBegin/glEnd ���� ���� �Լ����� �ӽ÷� �����մϴ�.
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

    // �÷��̾� �ʱ�ȭ
    player.Init({ 100.0, 300.0 });
}

void Engine::GameLoop()
{
    while (!glfwWindowShouldClose(p_window))
    {
        // 1. ��Ÿ Ÿ�� ���
        double currentFrameTime = glfwGetTime();
        deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;

        // 2. �Է� ó��
        glfwPollEvents();
        if (glfwGetKey(p_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(p_window, true);

        // 3. ���� ���� ������Ʈ
        Update();

        // 4. ������
        Draw();

        // 5. ���� ����
        glfwSwapBuffers(p_window);
    }
}

void Engine::Shutdown()
{
    glfwTerminate();
}

void Engine::Update()
{
    // �÷��̾� ������Ʈ (���� ��� ����)
    player.Update(deltaTime, p_window);
}

void Engine::Draw() const
{
    // ȭ���� ���������� Ŭ����
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 2D �������� ���� ��ǥ�� ����
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1280.0, 0.0, 720.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // �÷��̾� �׸���
    player.Draw();
}