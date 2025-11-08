#pragma once

namespace CS200
{
    struct RGBA;

    class RenderingAPI
    {
    public:
        static void Init();
        static void SetClearColor(RGBA color);
        static void Clear();
    };
}