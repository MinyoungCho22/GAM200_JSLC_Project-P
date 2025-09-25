#pragma once
#include "../Engine/Vec2.h" // 직접 만든 Vec2 헤더를 포함합니다.

// 전방 선언
struct GLFWwindow;

class Player
{
public:
    // Init 함수의 매개변수 타입을 Vec2로 변경
    void Init(Vec2 startPos);
    void Update(double dt, GLFWwindow* p_window);
    void Draw() const;

private:
    // 멤버 변수들의 타입을 모두 Vec2로 변경
    Vec2 position;
    Vec2 velocity;
    Vec2 size;

    bool is_on_ground = false;
};