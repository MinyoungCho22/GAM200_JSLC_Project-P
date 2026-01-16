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
    }

    /**
     * @brief Updates the input buffers. Should be called at the start of every frame.
     */
    void Input::Update()
    {
        // Move current frame states to the previous frame buffer
        m_keyStatePrevious = m_keyState;

        // Poll the status of every key supported by GLFW
        for (int key = 0; key <= GLFW_KEY_LAST; ++key)
        {
            // Update the current state of the key
            m_keyState[key] = glfwGetKey(m_window, key);
        }
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

} // namespace Input