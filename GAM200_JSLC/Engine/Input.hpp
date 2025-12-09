#pragma once
#include <array>
struct GLFWwindow;

namespace Input
{
    enum Key {
        // 실제 GLFW 키 코드와 동일한 값
        Space = 32,

        Num1 = 49,
        Num2 = 50,
        Num3 = 51,
        Num4 = 52,

        A = 65,
        D = 68,
        E = 69,
        F = 70,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        S = 83,
        W = 87,
        Escape = 256,
        Enter = 257,
        Tab = 258,

        Right = 262,
        Left = 263,
        Down = 264,
        Up = 265,

        LeftShift = 340,
        LeftControl = 341
    };

    class Input
    {
    public:
        Input() = default;
        void Initialize(GLFWwindow* window);
        void Update();

        bool IsKeyPressed(Key key) const;
        bool IsKeyTriggered(Key key) const;

    private:
        GLFWwindow* m_window = nullptr;

        std::array<int, 349> m_keyState{};
        std::array<int, 349> m_keyStatePrevious{};
    };
} // namespace Input