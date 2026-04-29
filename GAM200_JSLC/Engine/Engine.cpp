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

#include <chrono>
#include <thread>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
static Engine* g_emscripten_engine = nullptr;
static void EmscriptenStep() { g_emscripten_engine->Step(); }

// JS에서 호출: 캔버스 버퍼 크기를 변경하고 GLFW framebuffer resize 이벤트 발생시킴
extern "C" EMSCRIPTEN_KEEPALIVE
void game_resize_canvas(int w, int h)
{
    emscripten_set_canvas_element_size("#canvas", w, h);
}
#endif

#if defined(__linux__) && defined(GAM200_HAVE_XFIXES)
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

namespace
{
// Depth-1 empty pixmap cursor; WSLg sometimes ignores GLFW HIDDEN/XFixes alone (see microsoft/wslg#376).
unsigned long CreateInvisibleX11Cursor(Display* dpy)
{
    if (!dpy)
        return 0;
    const Window root = DefaultRootWindow(dpy);
    Pixmap empty = XCreatePixmap(dpy, root, 1, 1, 1);
    if (empty == None)
        return 0;
    GC gc = XCreateGC(dpy, empty, 0, nullptr);
    if (!gc)
    {
        XFreePixmap(dpy, empty);
        return 0;
    }
    XSetForeground(dpy, gc, 0);
    XSetBackground(dpy, gc, 0);
    XDrawPoint(dpy, empty, gc, 0, 0);
    XColor fg{};
    XColor bg{};
    fg.flags = bg.flags = DoRed | DoGreen | DoBlue;
    const Cursor c = XCreatePixmapCursor(dpy, empty, empty, &fg, &bg, 0, 0);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, empty);
    return static_cast<unsigned long>(c);
}
} // namespace
#endif
//
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
#elif defined(__EMSCRIPTEN__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
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
    // Emscripten: glfwSwapInterval before emscripten_set_main_loop triggers a harmless warning; skip it.
#ifndef __EMSCRIPTEN__
    glfwSwapInterval(0);
#endif

    glfwSetKeyCallback(m_window, Engine::KeyCallback);
    glfwSetFramebufferSizeCallback(m_window, Engine::FramebufferSizeCallback);
    glfwSetWindowFocusCallback(m_window, Engine::WindowFocusCallback);
    glfwSetCursorEnterCallback(m_window, Engine::CursorEnterCallback);

    m_input = std::make_unique<Input::Input>();
    m_input->Initialize(m_window);

    // Hide the default OS cursor so we can render our custom cursor
    ApplyCustomCursorHidden();

    m_controlBindings = std::make_unique<ControlBindings>();
    m_controlBindings->LoadOrDefaults("Config/control_bindings.json");

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
    m_postProcess->SetPresentationWindow(m_window);

    // On HiDPI/Retina displays (e.g. macOS), the framebuffer can be larger than the window size.
    // Fetch the real framebuffer dimensions and inform PostProcessManager.
    SyncPostProcessDisplaySize();

    m_imguiManager = std::make_unique<ImguiManager>();
    (void)m_imguiManager->Initialize();
    m_imguiManager->SetEngine(this);
    m_imguiManager->SetControlBindings(m_controlBindings.get());

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

#ifdef __EMSCRIPTEN__
    g_emscripten_engine = this;
    emscripten_set_main_loop(EmscriptenStep, 0, 0);
    // simulate_infinite_loop=0: main()이 정상 반환, 루프는 requestAnimationFrame으로 실행
    // Engine은 main.cpp의 static s_engine으로 살아있음 (-sEXIT_RUNTIME=0 필수)
#else
    double nextFrameDeadline = m_lastFrameTime;

    while (!glfwWindowShouldClose(m_window) && m_gameStateManager->HasState())
    {
        Step();

        // FPS cap: sleep to maintain target frame rate when VSync is off
        if (!m_vsyncEnabled && m_fpsCap > 0)
        {
            double targetFrameTime = 1.0 / static_cast<double>(m_fpsCap);
            if (nextFrameDeadline < m_lastFrameTime)
                nextFrameDeadline = m_lastFrameTime;
            nextFrameDeadline += targetFrameTime;

            double remaining = nextFrameDeadline - glfwGetTime();
            if (remaining > 0.0)
            {
#if defined(__linux__)
                constexpr double kSpinReserveSec = 0.0015;
                if (remaining > kSpinReserveSec)
                {
                    std::this_thread::sleep_for(
                        std::chrono::duration<double>(remaining - kSpinReserveSec));
                }
#endif
                while (glfwGetTime() < nextFrameDeadline)
                {
                    // Short spin tail for accuracy
                }
            }
            else if (remaining < -targetFrameTime * 2.0)
            {
                nextFrameDeadline = glfwGetTime();
            }
        }
        else
        {
            nextFrameDeadline = glfwGetTime();
        }
    }
#endif
}

void Engine::Step()
{
#ifdef __EMSCRIPTEN__
    if (glfwWindowShouldClose(m_window) || !m_gameStateManager->HasState())
    {
        emscripten_cancel_main_loop();
        return;
    }
#endif

    double currentFrameTime = glfwGetTime();
    m_deltaTime = currentFrameTime - m_lastFrameTime;
    m_lastFrameTime = currentFrameTime;

    glfwPollEvents();
    m_input->Update(m_deltaTime);
    if (m_controlBindings)
        m_controlBindings->TickRebindCapture(m_window, *m_input);
    Update();

    if (m_returnToSplashRequested)
    {
        m_returnToSplashRequested = false;
        if (m_postProcess)
        {
            m_postProcess->Settings() = PostProcessSettings{};
            m_postProcess->SetPassthrough(false);
        }
        if (m_gameStateManager)
        {
            m_gameStateManager->Clear();
            m_gameStateManager->PushState(std::make_unique<SplashState>(*m_gameStateManager));
        }
    }

#ifdef __APPLE__
    // Monitor/fullscreen transitions often commit after the first poll; reading FB size
    // immediately after Update() can still see the previous drawable (wrong letterbox).
    glfwPollEvents();
    // AppKit can restore the arrow cursor after Cmd+Tab even when focus callbacks ran.
    if (glfwGetWindowAttrib(m_window, GLFW_FOCUSED) &&
        glfwGetInputMode(m_window, GLFW_CURSOR) != GLFW_CURSOR_HIDDEN)
    {
        ApplyCustomCursorHidden();
    }
#endif

    SyncPostProcessDisplaySize();

    // When the top state bypasses post-processing (e.g. settings UI),
    // render everything to the FBO normally but present with exposure=1.0
    // so the UI is not affected by scene darkening effects.
    const bool bypass = m_gameStateManager->TopBypassesPostProcess();
    m_postProcess->SetPassthrough(bypass);

    m_postProcess->BeginScene();
    m_gameStateManager->Draw();
    m_postProcess->EndScene();
    m_postProcess->ApplyAndPresent();
    m_gameStateManager->DrawForegroundAfterPostProcess();

    m_postProcess->SetPassthrough(false);

    if (m_imguiManager && !m_isFullscreen)
    {
        m_imguiManager->BeginFrame(m_deltaTime);
        m_imguiManager->DrawDebugWindow();
        m_imguiManager->EndFrame();
    }

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
    if (m_window && glfwGetWindowAttrib(m_window, GLFW_FOCUSED))
        ApplyCustomCursorHidden();
#endif

    // WSL/Wayland/X11 sometimes reset swap interval after other contexts; re-apply on the main window.
    // In exclusive fullscreen, keep VSync on even if the menu option is off:
    // otherwise the moving tear line is very visible at QHD/144Hz+.
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval((m_vsyncEnabled || m_isFullscreen) ? 1 : 0);
    glfwSwapBuffers(m_window);
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
        // Borderless fullscreen always matches the monitor. Store the requested
        // size for the next windowed restore instead of switching video modes.
        SyncPostProcessDisplaySize();
        return;
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

    glfwPollEvents();
    SyncPostProcessDisplaySize();
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

void Engine::WindowFocusCallback(GLFWwindow* window, int focused)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine && focused)
    {
        // macOS / WSL(X11) may re-show the system cursor after focus changes; re-apply hide.
        engine->ApplyCustomCursorHidden();
    }
}

void Engine::CursorEnterCallback(GLFWwindow* window, int entered)
{
    Engine* engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    if (engine && entered)
        engine->ApplyCustomCursorHidden();
}

void Engine::ApplyCustomCursorHidden()
{
    if (!m_window)
        return;
#if defined(__linux__)
    // Do not use GLFW_CURSOR_DISABLED here: it is for FPS-style relative look and breaks 2D glfwGetCursorPos.
    glfwSetCursor(m_window, nullptr);
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#if defined(GAM200_HAVE_XFIXES)
    if (Display* dpy = glfwGetX11Display())
    {
        const Window xw = glfwGetX11Window(m_window);
        if (xw)
        {
            if (m_x11InvisibleCursor == 0)
                m_x11InvisibleCursor = CreateInvisibleX11Cursor(dpy);
            if (m_x11InvisibleCursor != 0)
            {
                XDefineCursor(dpy, xw, static_cast<Cursor>(m_x11InvisibleCursor));
                XFlush(dpy);
            }
            XFixesHideCursor(dpy, xw);
            XFlush(dpy);
            m_xfixesCursorHidden = true;
        }
    }
#endif
#else
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
#endif
}

void Engine::OnFramebufferResize(int newScreenWidth, int newScreenHeight)
{
    Logger::Instance().Log(Logger::Severity::Event,
        "Framebuffer resized to %d x %d", newScreenWidth, newScreenHeight);

    if (m_postProcess)
        m_postProcess->SyncPresentationFramebufferSizeFromWindow();
}

void Engine::SyncPostProcessDisplaySize()
{
    if (!m_window || !m_postProcess)
        return;

    m_postProcess->SyncPresentationFramebufferSizeFromWindow();
}

void Engine::ToggleFullscreen()
{
    m_isFullscreen = !m_isFullscreen;

    if (m_isFullscreen)
    {
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
        GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
        if (!mode)
            return;

        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(primaryMonitor, &monitorX, &monitorY);

        // Use borderless fullscreen instead of exclusive fullscreen. Exclusive
        // mode can flicker horizontal black lines on some Windows/QHD drivers.
        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(m_window, nullptr, monitorX, monitorY, mode->width, mode->height, 0);
        if (m_imguiManager)
            m_imguiManager->SetDebugWindowVisible(false);
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Borderless Fullscreen mode");
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

        glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, restoreW, restoreH, 0);
        if (m_imguiManager)
            m_imguiManager->SetDebugWindowVisible(true);
        if (shouldMaximizeWindowed)
        {
            glfwMaximizeWindow(m_window);
        }
        Logger::Instance().Log(Logger::Severity::Event, "Switched to Windowed mode");
    }

    glfwPollEvents();
    SyncPostProcessDisplaySize();
    ApplyCustomCursorHidden();
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
#ifdef __EMSCRIPTEN__
    // Browser rendering is tied to requestAnimationFrame; swap interval control is limited.
    // Keep this call for parity, but cap behavior is effectively controlled by FPS timing.
#endif
    glfwSwapInterval((enabled || m_isFullscreen) ? 1 : 0);

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
#ifdef __EMSCRIPTEN__
    // Web: apply cap using Emscripten main-loop timing.
    if (m_fpsCap > 0)
    {
        const int frameMs = (1000 + (m_fpsCap / 2)) / m_fpsCap;
        emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, frameMs > 0 ? frameMs : 1);
    }
    else
    {
        // "No Limit" on web maps to display refresh cadence (requestAnimationFrame).
        emscripten_set_main_loop_timing(EM_TIMING_RAF, 1);
    }
#endif
}

void Engine::RequestReturnToSplash()
{
    m_returnToSplashRequested = true;
}

void Engine::RequestShutdown()
{
    if (m_window)
    {
        glfwSetWindowShouldClose(m_window, true);
    }
#ifdef __EMSCRIPTEN__
    // Web builds do not actually "close a native window".
    // Stop the main loop, then try to close the tab; if blocked by browser policy,
    // clear the page to avoid a frozen last frame looking like a hang.
    emscripten_cancel_main_loop();
    EM_ASM({
        try {
            window.open("", "_self");
            window.close();
        } catch (e) {}
        setTimeout(function() {
            try {
                if (!window.closed) {
                    document.documentElement.style.background = "#000";
                    if (document.body) document.body.innerHTML = "";
                }
            } catch (e) {}
        }, 0);
    });
#endif
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
#if defined(__linux__) && defined(GAM200_HAVE_XFIXES)
        if (Display* dpy = glfwGetX11Display())
        {
            const Window xw = glfwGetX11Window(m_window);
            if (xw)
            {
                if (m_xfixesCursorHidden)
                {
                    XFixesShowCursor(dpy, xw);
                    m_xfixesCursorHidden = false;
                }
                if (m_x11InvisibleCursor != 0)
                {
                    XUndefineCursor(dpy, xw);
                    XFreeCursor(dpy, static_cast<Cursor>(m_x11InvisibleCursor));
                    m_x11InvisibleCursor = 0;
                    XFlush(dpy);
                }
            }
        }
#endif
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
