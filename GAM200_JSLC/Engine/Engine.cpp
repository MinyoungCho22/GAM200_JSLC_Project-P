// Engine.cpp

#include "Engine.hpp"
#include "Logger.hpp"
#include "GameStateManager.hpp"
#include "../Game/SplashState.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/Shader.hpp" 
#include <GLFW/glfw3.h>
#include <algorithm> 

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Initialize(const std::string& windowTitle)
{
    Logger::Instance().Initialize(Logger::Severity::Debug, true);
    Logger::Instance().Log(Logger::Severity::Event, "Engine Start");

    if (!glfwInit()) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLFW");
        return false;
    }

    // Request a modern OpenGL context (needed for GLSL #version 330 core shaders).
    // macOS typically supports up to OpenGL 4.1 core profile.
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    m_window = glfwCreateWindow(m_width, m_height, windowTitle.c_str(), nullptr, nullptr);

    if (!m_window) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);

    glfwSetKeyCallback(m_window, Engine::KeyCallback);
    glfwSetFramebufferSizeCallback(m_window, Engine::FramebufferSizeCallback);

    m_input = std::make_unique<Input::Input>();
    m_input->Initialize(m_window);

#ifdef USE_GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        Logger::Instance().Log(Logger::Severity::Error, "Failed to initialize GLEW");
        return false;
    }
#endif
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(GL::GetString(GL_VERSION)));

    // NOTE: Folder is `OpenGL/Shaders` (case-sensitive on macOS)
    m_textureShader = std::make_unique<Shader>("OpenGL/Shaders/simple.vert", "OpenGL/Shaders/simple.frag");
    m_textureShader->use();
    m_textureShader->setInt("ourTexture", 0);

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

        int windowWidth, windowHeight;
        glfwGetFramebufferSize(m_window, &windowWidth, &windowHeight);
        GL::Viewport(0, 0, windowWidth, windowHeight);
        GL::ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        GL::Clear(GL_COLOR_BUFFER_BIT);

        m_gameStateManager->Draw();

        glfwSwapBuffers(m_window);
    }
}

void Engine::Update()
{
    m_gameStateManager->Update(m_deltaTime);
}

Math::ivec2 Engine::GetRecommendedResolution()
{
    if (!m_window)
    {
        return { 1920, 1080 };
    }
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (primaryMonitor == NULL)
    {
        return { 1920, 1080 };
    }
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    return { mode->width, mode->height };
}

void Engine::SetResolution(int width, int height)
{
    m_windowedWidth = width;
    m_windowedHeight = height;

    if (m_isFullscreen)
    {
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        glfwSetWindowMonitor(m_window, primaryMonitor, 0, 0, width, height, mode->refreshRate);
    }
    else
    {
        glfwSetWindowSize(m_window, width, height);

        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        int xpos = (mode->width - width) / 2;
        int ypos = (mode->height - height) / 2;
        glfwSetWindowPos(m_window, xpos, ypos);
    }

    Logger::Instance().Log(Logger::Severity::Event, "Resolution set to %d x %d", width, height);
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

void Engine::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine)
    {
        if (width == 0 || height == 0)
        {
            return;
        }
        engine->OnFramebufferResize(width, height);
    }
}

void Engine::OnFramebufferResize(int newScreenWidth, int newScreenHeight)
{
    Logger::Instance().Log(Logger::Severity::Event,
        "Framebuffer resized to %d x %d", newScreenWidth, newScreenHeight);
}

void Engine::ToggleFullscreen()
{
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen)
    {
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
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
    Logger::Instance().Log(Logger::Severity::Info, "Engine Stopped");
}