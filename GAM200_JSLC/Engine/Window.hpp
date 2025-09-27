#pragma once
#include "Vec2.hpp"
#include "../CS200/RGBA.hpp" 
#include <string_view>
#include <functional>
#include <gsl/gsl>

// SDL�� ��ü ����� �����ϴ� ���, �ʿ��� Ÿ�Ը� ���� �����Ͽ� ������ �ð��� �����ϵ��� ����
struct SDL_Window;
typedef void* SDL_GLContext;
typedef union SDL_Event SDL_Event;

class Window
{
public:
    // â ����, SDL �� OpenGL �ʱ�ȭ�� ����
    void Initialize(std::string_view title);

    // �̺�Ʈ ó�� �� ���� ������ �����ϴ� ������ ������Ʈ �Լ�
    void Update();

    // â�� �������� ���θ� ��ȯ
    bool IsClosed() const;

    // ���� â�� ũ��(�ȼ� ����)�� ��ȯ
    Math::ivec2 GetSize() const;

    // â�� ����� Ư�� �������� ����
    void Clear(CS200::RGBA color = CS200::WHITE);

    // â�� ũ�⸦ ������ ����
    void ForceResize(int desired_width, int desired_height);

    // ���� SDL_Window �����͸� ��ȯ (ImGui �� �ٸ� ���̺귯���� ���� �� �ʿ�)
    SDL_Window* GetSDLWindow() const;

    // OpenGL ���ؽ�Ʈ�� ��ȯ
    SDL_GLContext GetGLContext() const;

    // �̺�Ʈ ó���� ���� �ݹ� �Լ� Ÿ�� ���� �� ���� �Լ�
    using WindowEventCallback = std::function<void(const SDL_Event&)>;
    void SetEventCallback(WindowEventCallback callback);

    // �⺻ ������ �� �Ҹ���
    Window() = default;
    ~Window();

    // ���� �� �̵��� �����Ͽ� ��ü�� ���ϼ��� ����
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept = delete;
    Window& operator=(Window&&) noexcept = delete;

private:
    // SDL ���� ���ҽ� �ڵ�
    gsl::owner<SDL_Window*>   sdlWindow = nullptr;
    gsl::owner<SDL_GLContext> glContext = nullptr;

    // â ���� ����
    Math::ivec2 size{ default_width, default_height };
    bool is_closed = false;

    // �̺�Ʈ �ݹ� �Լ� ������
    WindowEventCallback eventCallback;

    // �ʱ�ȭ�� ���� Helper �Լ�
    void setupSDLWindow(std::string_view title);
    void setupOpenGL();
    void processEvents();

    // â �⺻�� ���
    static constexpr int default_width = 1280;
    static constexpr int default_height = 720;
};