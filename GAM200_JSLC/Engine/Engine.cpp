#include "Engine.hpp"
#include "Logger.hpp"
#include "GameStateManager.hpp"
#include "../Game/SplashState.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <GLFW/glfw3.h>
#include <algorithm> // std::min, std::max를 위해 추가

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Initialize(const std::string& windowTitle)
{
    Logger::Instance().Log(Logger::Severity::Event, "Engine Start");

    if (!glfwInit()) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLFW");
        return false;
    }

    // ✅ [수정] m_width/m_height는 항상 가상 해상도(1920x1080)를 사용합니다.
    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), nullptr, nullptr);

    if (!m_window) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, KeyCallback);

    // ✅ 프레임버퍼 크기 변경 콜백을 등록합니다.
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);

    m_input = std::make_unique<Input::Input>();
    m_input->Initialize(m_window);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLEW");
        return false;
    }
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(GL::GetString(GL_VERSION)));

    // ✅ [수정] 뷰포트를 초기 창 크기(1920x1080)로 설정합니다.
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
        m_input->Update();
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

void Engine::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine)
    {
        if (key == GLFW_KEY_ENTER && action == GLFW_PRESS && mods == GLFW_MOD_ALT)
        {
            engine->ToggleFullscreen();
        }
    }
}

// ✅ 프레임버퍼 콜백 함수의 구현
void Engine::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine)
    {
        engine->OnFramebufferResize(width, height);
    }
}

// ✅ [수정] 뷰포트 종횡비 계산 로직 (가장 중요한 부분)
void Engine::OnFramebufferResize(int newScreenWidth, int newScreenHeight)
{
    float targetAspect = static_cast<float>(VIRTUAL_WIDTH) / static_cast<float>(VIRTUAL_HEIGHT); // 1.777... (16:9)
    float newAspect = static_cast<float>(newScreenWidth) / static_cast<float>(newScreenHeight);

    int newWidth, newHeight, xOffset, yOffset;

    if (newAspect > targetAspect) // 화면이 타겟보다 가로로 넓음 (예: 2560x1080)
    {
        // 높이를 기준으로 뷰포트 너비를 계산 (좌우 필러박스)
        newHeight = newScreenHeight;
        newWidth = static_cast<int>(newScreenHeight * targetAspect);
        xOffset = (newScreenWidth - newWidth) / 2;
        yOffset = 0;
    }
    else // 화면이 타겟보다 세로로 넓음 (예: 2560x1600)
    {
        // 너비를 기준으로 뷰포트 높이를 계산 (상하 레터박스)
        newWidth = newScreenWidth;
        newHeight = static_cast<int>(newScreenWidth / targetAspect);
        xOffset = 0;
        yOffset = (newScreenHeight - newHeight) / 2;
    }

    // OpenGL에게 계산된 뷰포트 영역(검은 여백이 제외된 중앙 영역)에만 그리도록 명령
    GL::Viewport(xOffset, yOffset, newWidth, newHeight);

    Logger::Instance().Log(Logger::Severity::Event, "Viewport updated to %d x %d with offset (%d, %d)", newWidth, newHeight, xOffset, yOffset);
}

void Engine::ToggleFullscreen()
{
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen)
    {
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        // m_windowedWidth/Height는 이미 1920x1080으로 고정되어 있음

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        glfwSetWindowMonitor(m_window, primaryMonitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Fullscreen mode");
    }
    else
    {
        glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, m_windowedWidth, m_windowedHeight, 0);
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Windowed mode");
    }
}

void Engine::RequestShutdown()
{
    if (m_window)
    {
        glfwSetWindowShouldClose(m_window, true);
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