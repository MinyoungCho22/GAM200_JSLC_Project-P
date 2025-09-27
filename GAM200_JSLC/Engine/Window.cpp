#include "Window.hpp"
#include "../CS200/RenderingAPI.hpp" 
#include "Logger.hpp" 
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "Vec2.hpp"
#include <stdexcept> 

// OpenGL 속성 설정을 위한 Helper 함수
namespace
{
    void hint_gl(SDL_GLattr attr, int value)
    {
        if (SDL_GL_SetAttribute(attr, value) != 0)
        {
            Logger::Instance().Log(Logger::Severity::Error, "Failed to set GL Attribute: %s", SDL_GetError());
        }
    }
}

Window::~Window()
{
    if (glContext != nullptr)
    {
        SDL_GL_DeleteContext(glContext);
    }

    if (sdlWindow != nullptr)
    {
        SDL_DestroyWindow(sdlWindow);
    }

    SDL_Quit();
}

void Window::Initialize(std::string_view title)
{
    Logger::Instance().Log(Logger::Severity::Event, "Initializing window: %s", title.data());

    try
    {
        setupSDLWindow(title);
        setupOpenGL();
    }
    catch (const std::runtime_error& e)
    {
        Logger::Instance().Log(Logger::Severity::Error, "Window initialization failed: %s", e.what());
        is_closed = true; // 초기화 실패 시 창을 닫힌 상태로 설정
        return;
    }

    SDL_GL_GetDrawableSize(sdlWindow, &size.x, &size.y);
    CS200::RenderingAPI::SetClearColor(CS200::BLACK);

    Logger::Instance().Log(Logger::Severity::Event, "Window initialized successfully");
    Logger::Instance().Log(Logger::Severity::Debug, "Window size: %dx%d", size.x, size.y);
}

void Window::Update()
{
    SDL_GL_SwapWindow(sdlWindow);
    processEvents();
}

bool Window::IsClosed() const
{
    return is_closed;
}

Math::ivec2 Window::GetSize() const
{
    return size;
}

void Window::Clear(CS200::RGBA color)
{
    CS200::RenderingAPI::SetClearColor(color);
    CS200::RenderingAPI::Clear();
}

void Window::ForceResize(int desired_width, int desired_height)
{
    SDL_SetWindowSize(sdlWindow, desired_width, desired_height);
    // 실제 크기는 이벤트 루프에서 업데이트되므로 여기서는 로그만 남기도록 하자
    Logger::Instance().Log(Logger::Severity::Event, "Window resize forced to: %dx%d", desired_width, desired_height);
}

SDL_Window* Window::GetSDLWindow() const
{
    return sdlWindow;
}

SDL_GLContext Window::GetGLContext() const
{
    return glContext;
}

void Window::SetEventCallback(WindowEventCallback callback)
{
    eventCallback = std::move(callback);
}

void Window::setupSDLWindow(std::string_view title)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        throw std::runtime_error(std::string("Failed to init SDL: ") + SDL_GetError());
    }

    hint_gl(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    hint_gl(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    hint_gl(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    hint_gl(SDL_GL_DOUBLEBUFFER, 1);
    hint_gl(SDL_GL_DEPTH_SIZE, 24);

    sdlWindow = SDL_CreateWindow(
        title.data(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        default_width,
        default_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (sdlWindow == nullptr)
    {
        throw std::runtime_error(std::string("Failed to create window: ") + SDL_GetError());
    }
}

void Window::setupOpenGL()
{
    glContext = SDL_GL_CreateContext(sdlWindow);
    if (glContext == nullptr)
    {
        throw std::runtime_error(std::string("Failed to create OpenGL context: ") + SDL_GetError());
    }

    SDL_GL_MakeCurrent(sdlWindow, glContext);

    if (glewInit() != GLEW_OK)
    {
        throw std::runtime_error("Unable to initialize GLEW");
    }

    // VSync 설정 (Adaptive VSync 시도 후 실패 시 일반 VSync로 전환)
    if (SDL_GL_SetSwapInterval(-1) < 0)
    {
        SDL_GL_SetSwapInterval(1);
    }

    CS200::RenderingAPI::Init();
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
}

void Window::processEvents()
{
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0)
    {
        if (eventCallback)
        {
            eventCallback(event);
        }

        switch (event.type)
        {
        case SDL_QUIT:
            is_closed = true;
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                is_closed = true;
            }
            else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                SDL_GL_GetDrawableSize(sdlWindow, &size.x, &size.y);
                glViewport(0, 0, size.x, size.y);
                Logger::Instance().Log(Logger::Severity::Debug, "Window resized to %dx%d", size.x, size.y);
            }
            break;
        }
    }
}