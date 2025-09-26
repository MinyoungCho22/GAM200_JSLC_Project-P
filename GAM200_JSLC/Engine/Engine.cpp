#include "Engine.hpp"
#include "Logger.hpp"
#include "../OpenGL/Shader.hpp"
#include "Matrix.hpp"
#include "Vec2.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Initialize(const std::string& windowTitle)
{
    Logger::Instance().Log(Logger::Severity::Event, "Engine Start");

    if (!glfwInit())
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), nullptr, nullptr);
    if (!m_window)
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);
    Logger::Instance().Log(Logger::Severity::Event, "Window created successfully");


    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLAD");
        return false;
    }
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));


    glViewport(0, 0, m_width, m_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shader = std::make_unique<Shader>("OpenGL/shaders/simple.vert", "OpenGL/shaders/simple.frag");
    m_solid_color_shader = std::make_unique<Shader>("OpenGL/shaders/solid_color.vert", "OpenGL/shaders/solid_color.frag");
    Logger::Instance().Log(Logger::Severity::Info, "Shaders loaded");

    float line_vertices[] = { -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f, -0.5f, -0.5f, -0.5f };

    glGenVertexArrays(1, &m_groundVAO);
    glGenBuffers(1, &m_groundVBO);
    glBindVertexArray(m_groundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_groundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(line_vertices), line_vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    const char* playerTexturePath = "Asset/player.png";
    m_player.Init({ 100.0f, 300.0f }, playerTexturePath);
    Logger::Instance().Log(Logger::Severity::Info, "Player initialized with texture: %s", playerTexturePath);


    Logger::Instance().Log(Logger::Severity::Event, "Engine initialization successful");
    return true;
}

void Engine::Draw() const
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // [수정] 네임스페이스 추가
    Math::Matrix projection = Math::Matrix::CreateOrtho(0.0f, static_cast<float>(m_width), 0.0f, static_cast<float>(m_height), -1.0f, 1.0f);

    m_shader->use();
    m_shader->setMat4("projection", projection);
    m_player.Draw(*m_shader);

    m_solid_color_shader->use();
    m_solid_color_shader->setMat4("projection", projection);

    // [수정] 네임스페이스 추가
    Math::Vec2 groundPosition = { m_width / 2.0f, 100.0f };
    Math::Vec2 groundSize = { static_cast<float>(m_width), 2.0f };
    Math::Matrix groundModel = Math::Matrix::CreateTranslation(groundPosition) * Math::Matrix::CreateScale(groundSize);

    m_solid_color_shader->setMat4("model", groundModel);
    m_solid_color_shader->setVec3("objectColor", 0.8f, 0.8f, 0.8f);

    glBindVertexArray(m_groundVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void Engine::Shutdown()
{
    m_player.Shutdown();
    glDeleteVertexArrays(1, &m_groundVAO);
    glDeleteBuffers(1, &m_groundVBO);

    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    Logger::Instance().Log(Logger::Severity::Event, "Engine Stopped");
}

void Engine::GameLoop()
{
    m_lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(m_window))
    {
        double currentFrameTime = glfwGetTime();
        m_deltaTime = currentFrameTime - m_lastFrameTime;
        m_lastFrameTime = currentFrameTime;

        glfwPollEvents();
        Update();
        Draw();
        glfwSwapBuffers(m_window);
    }
}

void Engine::Update()
{
    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS)
    {
        m_player.MoveLeft();
    }
    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS)
    {
        m_player.MoveRight();
    }
    if (glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS)
    {
        m_player.Jump();
    }
    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS)
    {
        m_player.Crouch();
    }
    else
    {
        m_player.StopCrouch();
    }
    if (glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
    {
        m_player.Dash();
    }

    m_player.Update(m_deltaTime);
}