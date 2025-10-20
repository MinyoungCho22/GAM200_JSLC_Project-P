#include "Engine.hpp"
#include "Logger.hpp"
#include "GameStateManager.hpp"
#include "../Game/SplashState.hpp"
#include "../OpenGL/GLWrapper.hpp"
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

    // â ���� �����ϱ� ���� ���ϴ� �ʺ�� ���̸� ���� �����մϴ�.
    m_width = 1980;
    m_height = 1080;
    m_windowedWidth = m_width;
    m_windowedHeight = m_height;

    // �� ��° ����(monitor)�� nullptr�� �ٲ㼭 â ���� ���ϴ�.
    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), nullptr, nullptr);

    if (!m_window) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    // ��üȭ�鿡�� ���ƿ� �� ����� �ʱ� â ��ġ�� �����մϴ�.
    glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);

    glfwMakeContextCurrent(m_window);

    // �ݹ� �Լ��� Engine�� ��� �Լ��� ������ �� �ֵ��� this �����͸� ����մϴ�.
    glfwSetWindowUserPointer(m_window, this);

    // KeyCallback �Լ��� GLFW�� Ű���� �ݹ����� ����մϴ�.
    glfwSetKeyCallback(m_window, KeyCallback);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLEW");
        return false;
    }
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(GL::GetString(GL_VERSION)));

    GL::Viewport(0, 0, m_width, m_height);
    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

        GL::Clear(GL_COLOR_BUFFER_BIT);

        m_gameStateManager->Draw();

        glfwSwapBuffers(m_window);
    }
}

void Engine::Update()
{
    m_gameStateManager->Update(m_deltaTime);
}

// Ű���� �Է��� �߻��� ������ ȣ��� �ݹ� �Լ�
void Engine::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // â�� ����� �� Engine ��ü�� �ٽ� ������
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine)
    {
        // Alt + Enter Ű�� '������ ��'�� ����
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && mods == GLFW_MOD_ALT)
        {
            engine->ToggleFullscreen();
        }
    }
}

// ���� ��üȭ��/â��� ��ȯ ����
void Engine::ToggleFullscreen()
{
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen)
    {
        // --- ��üȭ������ ��ȯ ---
        // 1. ���� â�� ��ġ�� ũ�⸦ �����մϴ�.
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);

        // 2. �� ������� �ػ󵵸� �����ɴϴ�.
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        // 3. ��üȭ������ ��ȯ�մϴ�.
        glfwSetWindowMonitor(m_window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Fullscreen mode");
    }
    else
    {
        // --- â ���� ��ȯ ---
        // ������ �� ��ġ�� ũ�⸦ ����� â ���� �ǵ����ϴ�.
        glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, m_windowedWidth, m_windowedHeight, 0);
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Windowed mode");
    }
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