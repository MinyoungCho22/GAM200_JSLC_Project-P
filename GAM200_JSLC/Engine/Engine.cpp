#include "Engine.hpp"
#include "Logger.hpp"
#include "GameStateManager.hpp"
#include "../Game/SplashState.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Initialize(const std::string& windowTitle)
{
    Logger::Instance().Log(Logger::Severity::Event, "Engine Start");

    if (!glfwInit()) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLFW");
        return false;
    }

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    m_width = mode->width;
    m_height = mode->height;
    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), primaryMonitor, nullptr);
    if (!m_window) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(m_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLAD");
        return false;
    }
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    glViewport(0, 0, m_width, m_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_gameStateManager = std::make_unique<GameStateManager>(*this);
    m_gameStateManager->PushState(std::make_unique<SplashState>(*m_gameStateManager));

    return true;
}

void Engine::GameLoop()
{
    m_lastFrameTime = glfwGetTime();
    while (!glfwWindowShouldClose(m_window) && m_gameStateManager->HasState())
    {
        double currentFrameTime = glfwGetTime();
        m_deltaTime = currentFrameTime - m_lastFrameTime;
        m_lastFrameTime = currentFrameTime;

        glfwPollEvents();
        Update();

        glClear(GL_COLOR_BUFFER_BIT);

        m_gameStateManager->Draw();

        glfwSwapBuffers(m_window);
    }
}

void Engine::Update()
{
    m_gameStateManager->Update(m_deltaTime);
}

void Engine::Shutdown()
{
    m_gameStateManager->Clear();
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    Logger::Instance().Log(Logger::Severity::Event, "Engine Stopped");
}