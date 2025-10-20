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

    // 창 모드로 실행하기 위해 원하는 너비와 높이를 직접 설정합니다.
    m_width = 1980;
    m_height = 1080;
    m_windowedWidth = m_width;
    m_windowedHeight = m_height;

    // 네 번째 인자(monitor)를 nullptr로 바꿔서 창 모드로 띄웁니다.
    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), nullptr, nullptr);

    if (!m_window) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    // 전체화면에서 돌아올 때 사용할 초기 창 위치를 저장합니다.
    glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);

    glfwMakeContextCurrent(m_window);

    // 콜백 함수가 Engine의 멤버 함수에 접근할 수 있도록 this 포인터를 등록합니다.
    glfwSetWindowUserPointer(m_window, this);

    // KeyCallback 함수를 GLFW에 키보드 콜백으로 등록합니다.
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

// 키보드 입력이 발생할 때마다 호출될 콜백 함수
void Engine::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // 창에 등록해 둔 Engine 객체를 다시 가져옴
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine)
    {
        // Alt + Enter 키가 '눌렸을 때'만 실행
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && mods == GLFW_MOD_ALT)
        {
            engine->ToggleFullscreen();
        }
    }
}

// 실제 전체화면/창모드 전환 로직
void Engine::ToggleFullscreen()
{
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen)
    {
        // --- 전체화면으로 전환 ---
        // 1. 현재 창의 위치와 크기를 저장합니다.
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);

        // 2. 주 모니터의 해상도를 가져옵니다.
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        // 3. 전체화면으로 전환합니다.
        glfwSetWindowMonitor(m_window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Fullscreen mode");
    }
    else
    {
        // --- 창 모드로 전환 ---
        // 저장해 둔 위치와 크기를 사용해 창 모드로 되돌립니다.
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