//Window.hpp

#pragma once
#include "Vec2.hpp"
#include "../CS200/RGBA.hpp" 
#include <string_view>
#include <functional>

// Forward declarations of SDL structures
struct SDL_Window;
typedef void* SDL_GLContext;
typedef union SDL_Event SDL_Event;

class Window
{
public:
    void Initialize(std::string_view title);
    void Update();
    bool IsClosed() const;
    Math::ivec2 GetSize() const;
    void Clear(CS200::RGBA color = CS200::WHITE);
    void ForceResize(int desired_width, int desired_height);
    
    SDL_Window* GetSDLWindow() const;
    SDL_GLContext GetGLContext() const;

    using WindowEventCallback = std::function<void(const SDL_Event&)>;
    void SetEventCallback(WindowEventCallback callback);

    Window() = default;
    ~Window();

    // Prevent copying and moving
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept = delete;
    Window& operator=(Window&&) noexcept = delete;

private:
    // Declare constants first to prevent initialization order issues
    static constexpr int default_width = 1280;
    static constexpr int default_height = 720;

    SDL_Window* sdlWindow = nullptr;
    SDL_GLContext glContext = nullptr;

    Math::ivec2 size{ default_width, default_height };
    bool is_closed = false;

    WindowEventCallback eventCallback;

    void setupSDLWindow(std::string_view title);
    void setupOpenGL();
    void processEvents();
};