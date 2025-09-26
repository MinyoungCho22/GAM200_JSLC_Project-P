#pragma once
#include "Vec2.hpp"
#include "../CS200/RGBA.hpp" // RGBA 색상 구조체를 사용하기 위해
#include <string_view>
#include <functional>
#include <gsl/gsl>

// SDL의 전체 헤더를 포함하는 대신, 필요한 타입만 전방 선언하여 컴파일 시간을 단축합니다.
struct SDL_Window;
typedef void* SDL_GLContext;
typedef union SDL_Event SDL_Event;

class Window
{
public:
    // 창 생성, SDL 및 OpenGL 초기화를 수행합니다.
    void Initialize(std::string_view title);

    // 이벤트 처리 및 버퍼 스왑을 수행하는 프레임 업데이트 함수입니다.
    void Update();

    // 창이 닫혔는지 여부를 반환합니다.
    bool IsClosed() const;

    // 현재 창의 크기(픽셀 단위)를 반환합니다.
    Math::ivec2 GetSize() const;

    // 창의 배경을 특정 색상으로 지웁니다.
    void Clear(CS200::RGBA color = CS200::WHITE);

    // 창의 크기를 강제로 변경합니다.
    void ForceResize(int desired_width, int desired_height);

    // 내부 SDL_Window 포인터를 반환합니다. (ImGui 등 다른 라이브러리와 연동 시 필요)
    SDL_Window* GetSDLWindow() const;

    // OpenGL 컨텍스트를 반환합니다.
    SDL_GLContext GetGLContext() const;

    // 이벤트 처리를 위한 콜백 함수 타입 정의 및 설정 함수입니다.
    using WindowEventCallback = std::function<void(const SDL_Event&)>;
    void SetEventCallback(WindowEventCallback callback);

    // 기본 생성자 및 소멸자
    Window() = default;
    ~Window();

    // 복사 및 이동을 금지하여 객체의 유일성을 보장합니다.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept = delete;
    Window& operator=(Window&&) noexcept = delete;

private:
    // SDL 관련 리소스 핸들
    gsl::owner<SDL_Window*>   sdlWindow = nullptr;
    gsl::owner<SDL_GLContext> glContext = nullptr;

    // 창 상태 변수
    Math::ivec2 size{ default_width, default_height };
    bool is_closed = false;

    // 이벤트 콜백 함수 포인터
    WindowEventCallback eventCallback;

    // 초기화를 위한 Helper 함수
    void setupSDLWindow(std::string_view title);
    void setupOpenGL();
    void processEvents();

    // 창 기본값 상수
    static constexpr int default_width = 1280;
    static constexpr int default_height = 720;
};