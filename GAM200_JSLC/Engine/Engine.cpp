// Engine.cpp

#include "Engine.hpp"
#include "Logger.hpp"
#include "GameStateManager.hpp"
#include "ImguiManager.hpp"
#include "DroneConfig.hpp"
#include "RobotConfig.hpp"
#include "../Game/SplashState.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../OpenGL/Shader.hpp"
#include <GLFW/glfw3.h>
#include <thread>
#include <chrono>

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

    // Center window on primary monitor (needed for macOS which doesn't auto-center)
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidmode = glfwGetVideoMode(primaryMonitor);
    if (vidmode)
    {
        int xpos = (vidmode->width  - m_width)  / 2;
        int ypos = (vidmode->height - m_height) / 2;
        glfwSetWindowPos(m_window, xpos, ypos);
    }

    glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
    glfwMakeContextCurrent(m_window);
    glfwSetWindowUserPointer(m_window, this);

    // Disable VSync on the main game context.
    glfwSwapInterval(0);

    glfwSetKeyCallback(m_window, Engine::KeyCallback);
    glfwSetFramebufferSizeCallback(m_window, Engine::FramebufferSizeCallback);

    // Hide the default OS cursor so we can render our custom cursor
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

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

    m_postProcess = std::make_unique<PostProcessManager>();
    m_postProcess->Initialize(m_width, m_height);

    // On HiDPI/Retina displays (e.g. macOS), the framebuffer can be larger than the window size.
    // Fetch the real framebuffer dimensions and inform PostProcessManager.
    {
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0)
            m_postProcess->SetDisplaySize(fbWidth, fbHeight);
    }

    m_imguiManager = std::make_unique<ImguiManager>();
    (void)m_imguiManager->Initialize();
    m_imguiManager->SetEngine(this);

    // Initialize drone config manager
    m_droneConfigManager = std::make_shared<DroneConfigManager>();
    m_imguiManager->SetDroneConfigManager(m_droneConfigManager);

    // Initialize robot config manager
    m_robotConfigManager = std::make_shared<RobotConfigManager>();
    m_imguiManager->SetRobotConfigManager(m_robotConfigManager);

    return true;
}

void Engine::GameLoop()
{
    m_lastFrameTime = glfwGetTime();
    double nextFrameDeadline = m_lastFrameTime;

    while (!glfwWindowShouldClose(m_window) && m_gameStateManager->HasState())
    {
        double currentFrameTime = glfwGetTime();
        m_deltaTime = currentFrameTime - m_lastFrameTime;
        m_lastFrameTime = currentFrameTime;

        glfwPollEvents();
        m_input->Update();
        Update();

        // Sync display size every frame — covers Windows DPI scaling and macOS Retina
        // cases where the physical framebuffer may differ from the logical window size.
        {
            int fbW = 0, fbH = 0;
            glfwGetFramebufferSize(m_window, &fbW, &fbH);
            if (fbW > 0 && fbH > 0)
                m_postProcess->SetDisplaySize(fbW, fbH);
        }

        // When the top state bypasses post-processing (e.g. settings UI),
        // render everything to the FBO normally but present with exposure=1.0
        // so the UI is not affected by scene darkening effects.
        const bool bypass = m_gameStateManager->TopBypassesPostProcess();
        m_postProcess->SetPassthrough(bypass);

        m_postProcess->BeginScene();
        m_gameStateManager->Draw();
        m_postProcess->EndScene();
        m_postProcess->ApplyAndPresent();

        m_postProcess->SetPassthrough(false);

        if (m_imguiManager)
        {
            m_imguiManager->BeginFrame(m_deltaTime);
            m_imguiManager->DrawDebugWindow();
            m_imguiManager->EndFrame();
        }

        glfwSwapBuffers(m_window);

        // FPS cap: sleep to maintain target frame rate when VSync is off
        if (!m_vsyncEnabled && m_fpsCap > 0)
        {
            double targetFrameTime = 1.0 / static_cast<double>(m_fpsCap);
            // Use absolute deadline scheduling to reduce jitter/drift at 60/144 caps.
            if (nextFrameDeadline < currentFrameTime)
            {
                nextFrameDeadline = currentFrameTime;
            }
            nextFrameDeadline += targetFrameTime;

            double remaining = nextFrameDeadline - glfwGetTime();
            if (remaining > 0.0)
            {
                // Accuracy-first limiter: avoid sleep/yield overshoot on Windows scheduler.
                while (glfwGetTime() < nextFrameDeadline)
                {
                    // Busy wait for precise cap timing (especially stable at 30/60/144).
                }
            }
            else if (remaining < -targetFrameTime * 2.0)
            {
                // If we fall far behind, resync to avoid long-term phase error.
                nextFrameDeadline = glfwGetTime();
            }
        }
        else
        {
            // Keep deadline aligned when cap is disabled or vsync is enabled.
            nextFrameDeadline = glfwGetTime();
        }
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

    if (m_postProcess)
        m_postProcess->SetDisplaySize(newScreenWidth, newScreenHeight);
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
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);

        int restoreW = m_windowedWidth;
        int restoreH = m_windowedHeight;
        bool shouldMaximizeWindowed = false;

        // If saved windowed size is monitor-sized (e.g. 1920x1080 on 1920 monitor),
        // restore as maximized windowed mode so it is visibly windowed (not true fullscreen).
        if (mode && restoreW >= mode->width && restoreH >= mode->height)
        {
            shouldMaximizeWindowed = true;
            restoreW = mode->width;
            restoreH = mode->height;
        }

        glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, restoreW, restoreH, 0);
        if (shouldMaximizeWindowed)
        {
            glfwMaximizeWindow(m_window);
        }
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Windowed mode");
    }
}

void Engine::SetFullscreen(bool enabled)
{
    if (m_isFullscreen == enabled) return;
    ToggleFullscreen();
}

void Engine::SetVSync(bool enabled)
{
    m_vsyncEnabled = enabled;
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(enabled ? 1 : 0);

    // Ensure debug window never adds an extra vsync wait.
    if (m_imguiManager && m_imguiManager->IsInitialized())
    {
        m_imguiManager->ForceDebugSwapIntervalOff();
        glfwMakeContextCurrent(m_window);
    }
}

void Engine::SetFpsCap(int cap)
{
    m_fpsCap = cap;
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

    if (m_imguiManager)
    {
        m_imguiManager->Shutdown();
        m_imguiManager.reset();
    }

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    if (m_postProcess)
    {
        m_postProcess->Shutdown();
        m_postProcess.reset();
    }

    glfwTerminate();
    Logger::Instance().Log(Logger::Severity::Info, "Engine Stopped");
}
