#pragma once
#include "Vec2.hpp"

namespace Math
{
    // float Ÿ���� ����ϴ� �簢�� ����ü
    struct Rect
    {
        Vec2 bottom_left{ 0.0f, 0.0f };
        Vec2 top_right{ 0.0f, 0.0f };
    };

    // int Ÿ���� ����ϴ� �簢�� ����ü
    struct IRect
    {
        ivec2 bottom_left{ 0, 0 };
        ivec2 top_right{ 0, 0 };
    };
}