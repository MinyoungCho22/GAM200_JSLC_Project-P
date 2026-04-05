// Input.cpp

#include "Input.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Input
{
    namespace
    {
        // Right stick → virtual cursor (framebuffer pixels/sec at full deflection before curve).
        constexpr double kGamepadAimBaseSpeed = 2200.0;
        constexpr float kGamepadAimDeadzone = 0.18f;
        // >1: finer control when barely tilting the stick.
        constexpr float kGamepadAimCurveExponent = 1.38f;
        // Velocity low-pass toward stick target (higher = snappier, lower = smoother).
        constexpr double kGamepadAimSmoothLambda = 16.0;
        constexpr float kGamepadStickMaxMag = 1.41421356f; // sqrt(2) for per-axis ±1 pads
    }

    void Input::SetGamepadAimSensitivity(float sensitivity)
    {
        m_gamepadAimSensitivity = std::max(0.1f, std::min(sensitivity, 4.0f));
    }

    void Input::Initialize(GLFWwindow* window)
    {
        m_window = window;

        m_keyState.fill(GLFW_RELEASE);
        m_keyStatePrevious.fill(GLFW_RELEASE);
        m_mouseButtonState.fill(GLFW_RELEASE);
        m_mouseButtonStatePrevious.fill(GLFW_RELEASE);
        std::memset(&m_gamepadCurr, 0, sizeof(m_gamepadCurr));
        std::memset(&m_gamepadPrev, 0, sizeof(m_gamepadPrev));
        m_gamepadAimVelX = 0.0;
        m_gamepadAimVelY = 0.0;
    }

    void Input::Update(double dt)
    {
        m_keyStatePrevious = m_keyState;
        m_mouseButtonStatePrevious = m_mouseButtonState;
        m_gamepadPrev = m_gamepadCurr;

        for (int key = 0; key <= GLFW_KEY_LAST; ++key)
            m_keyState[static_cast<size_t>(key)] = glfwGetKey(m_window, key);

        for (int button = 0; button < 8; ++button)
            m_mouseButtonState[static_cast<size_t>(button)] = glfwGetMouseButton(m_window, button);

        double windowMouseX = 0.0;
        double windowMouseY = 0.0;
        glfwGetCursorPos(m_window, &windowMouseX, &windowMouseY);

        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(m_window, &windowWidth, &windowHeight);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);

        if (windowWidth > 0 && windowHeight > 0)
        {
            const double scaleX = static_cast<double>(framebufferWidth) / static_cast<double>(windowWidth);
            const double scaleY = static_cast<double>(framebufferHeight) / static_cast<double>(windowHeight);
            m_mouseX = windowMouseX * scaleX;
            m_mouseY = windowMouseY * scaleY;
        }
        else
        {
            m_mouseX = windowMouseX;
            m_mouseY = windowMouseY;
        }

        m_gamepadConnected = glfwJoystickPresent(GLFW_JOYSTICK_1) != 0
            && glfwJoystickIsGamepad(GLFW_JOYSTICK_1) != 0;
        if (m_gamepadConnected)
        {
            if (glfwGetGamepadState(GLFW_JOYSTICK_1, &m_gamepadCurr) != GLFW_TRUE)
                std::memset(&m_gamepadCurr, 0, sizeof(m_gamepadCurr));
        }
        else
        {
            std::memset(&m_gamepadCurr, 0, sizeof(m_gamepadCurr));
            m_gamepadAimVelX = 0.0;
            m_gamepadAimVelY = 0.0;
        }

        // Right stick moves the logical cursor (same space as mouse) for targeting with a gamepad.
        if (m_gamepadConnected && framebufferWidth > 0 && framebufferHeight > 0)
        {
            const float rx = m_gamepadCurr.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
            const float ry = m_gamepadCurr.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
            const float mag = std::sqrt(rx * rx + ry * ry);

            double targetVx = 0.0;
            double targetVy = 0.0;
            if (mag > kGamepadAimDeadzone)
            {
                const float dirX = rx / mag;
                const float dirY = ry / mag;
                const float denom = (std::max)(1.0e-4f, kGamepadStickMaxMag - kGamepadAimDeadzone);
                const float rawT = (mag - kGamepadAimDeadzone) / denom;
                const float t = (std::min)(1.0f, (std::max)(0.0f, rawT));
                const float curved = std::pow(t, kGamepadAimCurveExponent);
                const double speed =
                    static_cast<double>(curved) * kGamepadAimBaseSpeed * static_cast<double>(m_gamepadAimSensitivity);
                targetVx = static_cast<double>(dirX) * speed;
                targetVy = static_cast<double>(dirY) * speed;
            }

            const double smoothAlpha = 1.0 - std::exp(-kGamepadAimSmoothLambda * dt);
            m_gamepadAimVelX += (targetVx - m_gamepadAimVelX) * smoothAlpha;
            m_gamepadAimVelY += (targetVy - m_gamepadAimVelY) * smoothAlpha;

            if (std::fabs(m_gamepadAimVelX) > 1e-3 || std::fabs(m_gamepadAimVelY) > 1e-3)
            {
                m_mouseX += m_gamepadAimVelX * dt;
                m_mouseY += m_gamepadAimVelY * dt;
                m_mouseX = (std::max)(0.0, (std::min)(static_cast<double>(framebufferWidth - 1), m_mouseX));
                m_mouseY = (std::max)(0.0, (std::min)(static_cast<double>(framebufferHeight - 1), m_mouseY));

                const double scaleX = static_cast<double>(framebufferWidth) / static_cast<double>(windowWidth);
                const double scaleY = static_cast<double>(framebufferHeight) / static_cast<double>(windowHeight);
                glfwSetCursorPos(m_window, m_mouseX / scaleX, m_mouseY / scaleY);
            }
        }
        else
        {
            m_gamepadAimVelX = 0.0;
            m_gamepadAimVelY = 0.0;
        }
    }

    bool Input::IsKeyPressed(Key key) const
    {
        return m_keyState.at(static_cast<size_t>(key)) == GLFW_PRESS;
    }

    bool Input::IsKeyTriggered(Key key) const
    {
        return m_keyState.at(static_cast<size_t>(key)) == GLFW_PRESS
            && m_keyStatePrevious.at(static_cast<size_t>(key)) == GLFW_RELEASE;
    }

    bool Input::IsGlfwKeyPressed(int glfwKey) const
    {
        if (glfwKey < 0 || glfwKey >= static_cast<int>(m_keyState.size()))
            return false;
        return m_keyState[static_cast<size_t>(glfwKey)] == GLFW_PRESS;
    }

    bool Input::IsGlfwKeyTriggered(int glfwKey) const
    {
        if (glfwKey < 0 || glfwKey >= static_cast<int>(m_keyState.size()))
            return false;
        return m_keyState[static_cast<size_t>(glfwKey)] == GLFW_PRESS
            && m_keyStatePrevious[static_cast<size_t>(glfwKey)] == GLFW_RELEASE;
    }

    bool Input::IsMouseButtonPressed(MouseButton button) const
    {
        return m_mouseButtonState.at(static_cast<size_t>(button)) == GLFW_PRESS;
    }

    bool Input::IsMouseButtonTriggered(MouseButton button) const
    {
        return m_mouseButtonState.at(static_cast<size_t>(button)) == GLFW_PRESS
            && m_mouseButtonStatePrevious.at(static_cast<size_t>(button)) == GLFW_RELEASE;
    }

    bool Input::IsCrouchHeld() const
    {
        if (IsKeyPressed(Key::S))
            return true;
        if (m_gamepadConnected)
        {
            // GLFW: left stick Y positive = down (same as ControlBindings move deadzone).
            constexpr float dz = 0.2f;
            const float ly = m_gamepadCurr.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
            if (ly > dz)
                return true;
        }
        return false;
    }

    void Input::GetMousePosition(double& x, double& y) const
    {
        x = m_mouseX;
        y = m_mouseY;
    }

} // namespace Input
