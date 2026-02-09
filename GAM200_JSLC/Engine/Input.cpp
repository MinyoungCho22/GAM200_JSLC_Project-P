// Input.cpp

#include "Input.hpp"
#include <GLFW/glfw3.h>

namespace Input
{
    /**
     * @brief Initializes the input system by attaching a window and resetting key states.
     * @param window The GLFW window to poll input from.
     */
    void Input::Initialize(GLFWwindow* window)
    {
        m_window = window;

        // Initialize both current and previous buffers to a released state
        m_keyState.fill(GLFW_RELEASE);
        m_keyStatePrevious.fill(GLFW_RELEASE);
        m_mouseButtonState.fill(GLFW_RELEASE);
        m_mouseButtonStatePrevious.fill(GLFW_RELEASE);
    }

    /**
     * @brief Updates the input buffers. Should be called at the start of every frame.
     */
    void Input::Update()
    {
        // Move current frame states to the previous frame buffer
        m_keyStatePrevious = m_keyState;
        m_mouseButtonStatePrevious = m_mouseButtonState;

        // Poll the status of every key supported by GLFW
        for (int key = 0; key <= GLFW_KEY_LAST; ++key)
        {
            // Update the current state of the key
            m_keyState[key] = glfwGetKey(m_window, key);
        }

        // Poll mouse button states
        for (int button = 0; button < 8; ++button)
        {
            m_mouseButtonState[button] = glfwGetMouseButton(m_window, button);
        }

        // Poll mouse position
        glfwGetCursorPos(m_window, &m_mouseX, &m_mouseY);
    }

    /**
     * @brief Checks if a key is currently held down.
     * @param key The enum code of the key to check.
     * @return True if the key is currently in a pressed state.
     */
    bool Input::IsKeyPressed(Key key) const
    {
        return m_keyState.at(key) == GLFW_PRESS;
    }

    /**
     * @brief Detects a new key press (Edge Triggering).
     * @param key The enum code of the key to check.
     * @return True only if the key was released in the last frame and pressed in this frame.
     */
    bool Input::IsKeyTriggered(Key key) const
    {
        // Detect the transition from RELEASE (previous) to PRESS (current)
        return (m_keyState.at(key) == GLFW_PRESS && m_keyStatePrevious.at(key) == GLFW_RELEASE);
    }

    /**
     * @brief Checks if a mouse button is currently held down.
     * @param button The mouse button to check.
     * @return True if the button is currently in a pressed state.
     */
    bool Input::IsMouseButtonPressed(MouseButton button) const
    {
        return m_mouseButtonState.at(static_cast<int>(button)) == GLFW_PRESS;
    }

    /**
     * @brief Detects a new mouse button press (Edge Triggering).
     * @param button The mouse button to check.
     * @return True only if the button was released in the last frame and pressed in this frame.
     */
    bool Input::IsMouseButtonTriggered(MouseButton button) const
    {
        return (m_mouseButtonState.at(static_cast<int>(button)) == GLFW_PRESS && 
                m_mouseButtonStatePrevious.at(static_cast<int>(button)) == GLFW_RELEASE);
    }

    /**
     * @brief Gets the current mouse position in screen coordinates.
     * @param x Reference to store the x coordinate.
     * @param y Reference to store the y coordinate.
     */
    void Input::GetMousePosition(double& x, double& y) const
    {
        x = m_mouseX;
        y = m_mouseY;
    }

} // namespace Input