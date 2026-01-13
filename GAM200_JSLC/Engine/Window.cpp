//Window.cpp

#include "Window.hpp"

// Include Logger immediately after Window.hpp
#include "Logger.hpp" 

#include "../CS200/RenderingAPI.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "Vec2.hpp"

// Usually <SDL2/SDL.h> when using vcpkg. 
// If the file is not found, try changing it to <SDL.h>.
#include <SDL2/SDL.h> 
#include <stdexcept>
#include <GL/glew.h> // Required for glewInit()

namespace
{
    void hint_gl(SDL_GLattr attr, int value)
    {
        if (SDL_GL_SetAttribute(attr, value) != 0)
        {
            // If Logger is included correctly, the error below will be valid.
            // Logger::Instance().Log(Logger::Severity::Error, "Failed to set GL Attribute: %s", SDL_GetError());
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
    // Logger::Instance().Log(Logger::Severity::Event, "Initializing window: %s", title.data());

    try
    {
        setupSDLWindow(title);
        setupOpenGL();
    }
    catch (const std::runtime_error& e)
    {
        // Logger::Instance().Log(Logger::Severity::Error, "Window initialization failed: %s", e.what());
        is_closed = true;
        return;
    }

    // sdlWindow must be valid when using SDL_GL_GetDrawableSize
    if (sdlWindow) 
    {
        SDL_GL_GetDrawableSize(sdlWindow, &size.x, &size.y);
    }
    
    CS200::RenderingAPI::SetClearColor(CS200::BLACK);

    // Logger::Instance().Log(Logger::Severity::Event, "Window initialized successfully");
    // Logger::Instance().Log(Logger::Severity::Debug, "Window size: %dx%d", size.x, size.y);
}

void Window::Update()
{
    if(sdlWindow)
    {
        SDL_GL_SwapWindow(sdlWindow);
    }
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
    if (sdlWindow)
    {
        SDL_SetWindowSize(sdlWindow, desired_width, desired_height);
        Logger::Instance().Log(Logger::Severity::Event, "Window resize forced to: %dx%d", desired_width, desired_height);
    }
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

    // Initialize GLEW (requires glew.h)
    glewExperimental = GL_TRUE; 
    if (glewInit() != GLEW_OK)
    {
        throw std::runtime_error("Unable to initialize GLEW");
    }

    if (SDL_GL_SetSwapInterval(-1) < 0)
    {
        SDL_GL_SetSwapInterval(1);
    }

    CS200::RenderingAPI::Init();
    
    const GLubyte* version = glGetString(GL_VERSION);
    Logger::Instance().Log(Logger::Severity::Debug, "OpenGL Version: %s", version ? reinterpret_cast<const char*>(version) : "Unknown");
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