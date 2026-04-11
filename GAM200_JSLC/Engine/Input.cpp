// Input.cpp

#include "Input.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

namespace Input
{
    namespace
    {
        // Right stick → virtual cursor (framebuffer pixels/sec at full deflection before curve).
        constexpr double kGamepadAimBaseSpeed = 2200.0;
        constexpr float kGamepadAimDeadzone = 0.18f;
        // Higher = finer micro-aim on stick (pad-only curve in Update).
        constexpr float kGamepadAimCurveExponent = 2.25f;
        // Velocity low-pass toward stick target (higher = snappier, lower = smoother).
        constexpr double kGamepadAimSmoothLambda = 16.0;
        constexpr float kGamepadStickMaxMag = 1.41421356f; // sqrt(2) for per-axis ±1 pads

        void GetCursorToFramebufferScale(GLFWwindow* window, int fbW, int fbH, double& outScaleX, double& outScaleY)
        {
            outScaleX = 1.0;
            outScaleY = 1.0;
            if (!window || fbW <= 0 || fbH <= 0)
                return;

#ifdef __EMSCRIPTEN__
            // Web: cursor position tracks CSS pixels of #canvas, while rendering uses framebuffer pixels.
            // Convert using real CSS size to avoid X-offset mismatch on scaled canvas.
            double cssW = 0.0;
            double cssH = 0.0;
            if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS
                && cssW > 0.0 && cssH > 0.0)
            {
                outScaleX = static_cast<double>(fbW) / cssW;
                outScaleY = static_cast<double>(fbH) / cssH;
                return;
            }
#endif

            int winW = 0;
            int winH = 0;
            glfwGetWindowSize(window, &winW, &winH);
            if (winW > 0 && winH > 0)
            {
                outScaleX = static_cast<double>(fbW) / static_cast<double>(winW);
                outScaleY = static_cast<double>(fbH) / static_cast<double>(winH);
            }
        }
    }

    void Input::SetGamepadAimSensitivity(float sensitivity)
    {
        m_gamepadAimSensitivity = std::max(0.1f, std::min(sensitivity, 4.0f));
    }

    void Input::SetMouseFramebufferPosition(double framebufferX, double framebufferY)
    {
        if (!m_window)
            return;
        int fbW = 0;
        int fbH = 0;
        glfwGetFramebufferSize(m_window, &fbW, &fbH);
        if (fbW <= 0 || fbH <= 0)
            return;
        m_mouseX = (std::max)(0.0, (std::min)(static_cast<double>(fbW - 1), framebufferX));
        m_mouseY = (std::max)(0.0, (std::min)(static_cast<double>(fbH - 1), framebufferY));
        double scaleX = 1.0;
        double scaleY = 1.0;
        GetCursorToFramebufferScale(m_window, fbW, fbH, scaleX, scaleY);
        glfwSetCursorPos(m_window, m_mouseX / scaleX, m_mouseY / scaleY);
    }

    void Input::ClearGamepadAimVelocity()
    {
        m_gamepadAimVelX = 0.0;
        m_gamepadAimVelY = 0.0;
    }

    bool Input::IsGamepadButtonPressed(int glfwGamepadButton) const
    {
        if (!m_gamepadConnected || glfwGamepadButton < 0 || glfwGamepadButton > GLFW_GAMEPAD_BUTTON_LAST)
            return false;
        return m_gamepadCurr.buttons[static_cast<size_t>(glfwGamepadButton)] == GLFW_PRESS;
    }

    bool Input::IsGamepadButtonTriggered(int glfwGamepadButton) const
    {
        if (!m_gamepadConnected || glfwGamepadButton < 0 || glfwGamepadButton > GLFW_GAMEPAD_BUTTON_LAST)
            return false;
        const size_t i = static_cast<size_t>(glfwGamepadButton);
        return m_gamepadCurr.buttons[i] == GLFW_PRESS && m_gamepadPrev.buttons[i] == GLFW_RELEASE;
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

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(m_window, &framebufferWidth, &framebufferHeight);
        double scaleX = 1.0;
        double scaleY = 1.0;
        GetCursorToFramebufferScale(m_window, framebufferWidth, framebufferHeight, scaleX, scaleY);
        m_mouseX = windowMouseX * scaleX;
        m_mouseY = windowMouseY * scaleY;

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

                double scaleX = 1.0;
                double scaleY = 1.0;
                GetCursorToFramebufferScale(m_window, framebufferWidth, framebufferHeight, scaleX, scaleY);
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
