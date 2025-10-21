#pragma once
#include "Vec2.hpp"

namespace Math
{
    // float 타입을 사용하는 사각형 구조체
    struct Rect
    {
        Vec2 bottom_left{ 0.0f, 0.0f };
        Vec2 top_right{ 0.0f, 0.0f };
    };

    // int 타입을 사용하는 사각형 구조체
    struct IRect
    {
        ivec2 bottom_left{ 0, 0 };
        ivec2 top_right{ 0, 0 };
    };
}