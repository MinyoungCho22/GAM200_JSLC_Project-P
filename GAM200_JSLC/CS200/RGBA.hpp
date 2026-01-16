//RGBA.hpp

#pragma once

namespace CS200
{
    struct RGBA
    {
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
    };

    constexpr RGBA BLACK{ 0.0f, 0.0f, 0.0f, 1.0f };
    constexpr RGBA WHITE{ 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr RGBA RED{ 1.0f, 0.0f, 0.0f, 1.0f };
    constexpr RGBA GREEN{ 0.0f, 1.0f, 0.0f, 1.0f };
    constexpr RGBA BLUE{ 0.0f, 0.0f, 1.0f, 1.0f };
}