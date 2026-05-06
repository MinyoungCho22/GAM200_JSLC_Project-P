// Input.hpp

#pragma once
// Prevent GLFW from including platform GL headers before GLEW (Windows gl.h breaks glew.h).
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <array>

namespace Input
{
    enum Key {
        Space = 32,

        Num1 = 49,
        Num2 = 50,
        Num3 = 51,
        Num4 = 52,
        Num5 = 53,

        A = 65,
        D = 68,
        E = 69,
        F = 70,
        I = 73,
        J = 74,
        K = 75,
        L = 76,
        Q = 81,
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

    enum class MouseButton {
        Left = 0,
        Right = 1,
        Middle = 2
    };

    class Input
    {
    public:
        void Initialize(GLFWwindow* window);

        void Update(double dt);

        bool IsKeyPressed(Key key) const;
        bool IsKeyTriggered(Key key) const;

        bool IsGlfwKeyPressed(int glfwKey) const;
        bool IsGlfwKeyTriggered(int glfwKey) const;

        bool IsMouseButtonPressed(MouseButton button) const;
        bool IsMouseButtonTriggered(MouseButton button) const;

        /** Crouch: S key or gamepad left stick down (same threshold as horizontal move stick). */
        bool IsCrouchHeld() const;

        void GetMousePosition(double& x, double& y) const;

        double GetMouseX() const { return m_mouseX; }
        double GetMouseY() const { return m_mouseY; }

        bool IsGamepadConnected() const { return m_gamepadConnected; }
        const GLFWgamepadstate& GamepadCurrent() const { return m_gamepadCurr; }
        const GLFWgamepadstate& GamepadPrevious() const { return m_gamepadPrev; }

        /** Right-stick virtual cursor speed multiplier (default 1; typical UI range 0.5–2). */
        void SetGamepadAimSensitivity(float sensitivity);
        float GetGamepadAimSensitivity() const { return m_gamepadAimSensitivity; }

        /** Move logical cursor in framebuffer pixel space (matches GetMousePosition). */
        void SetMouseFramebufferPosition(double framebufferX, double framebufferY);
        void ClearGamepadAimVelocity();

        bool IsGamepadButtonPressed(int glfwGamepadButton) const;
        bool IsGamepadButtonTriggered(int glfwGamepadButton) const;

    private:
        GLFWwindow* m_window = nullptr;

        std::array<int, 349> m_keyState{};
        std::array<int, 349> m_keyStatePrevious{};

        std::array<int, 8> m_mouseButtonState{};
        std::array<int, 8> m_mouseButtonStatePrevious{};

        double m_mouseX = 0.0;
        double m_mouseY = 0.0;

        bool m_gamepadConnected = false;
        GLFWgamepadstate m_gamepadCurr{};
        GLFWgamepadstate m_gamepadPrev{};

        float m_gamepadAimSensitivity = 1.0f;
        double m_gamepadAimVelX = 0.0;
        double m_gamepadAimVelY = 0.0;
    };
} // namespace Input
