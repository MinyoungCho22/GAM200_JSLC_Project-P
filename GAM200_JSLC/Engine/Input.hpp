// Input.hpp

#pragma once
#include <array>

// Forward declaration of the GLFW window structure
struct GLFWwindow;

namespace Input
{
    /**
     * @enum Key
     * @brief Key codes mapped to GLFW standards for keyboard input.
     */
    enum Key {
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

    /**
     * @class Input
     * @brief Manages keyboard input states, allowing for polling of held and triggered keys.
     */
    class Input
    {
    public:
        Input() = default;

        /**
         * @brief Attaches the input manager to a specific GLFW window context.
         * @param window Pointer to the GLFW window.
         */
        void Initialize(GLFWwindow* window);

        /**
         * @brief Updates current and previous key states. Should be called once per frame.
         */
        void Update();

        /**
         * @brief Checks if a key is currently being held down.
         * @param key The key to check.
         * @return True if the key is currently pressed.
         */
        bool IsKeyPressed(Key key) const;

        /**
         * @brief Checks if a key was pressed exactly during this frame.
         * @param key The key to check.
         * @return True only on the frame the key was first pressed.
         */
        bool IsKeyTriggered(Key key) const;

    private:
        GLFWwindow* m_window = nullptr;

        // Buffers to store key states for the current and previous frames.
        // Used to detect edge triggers (Transition from Released to Pressed).
        std::array<int, 349> m_keyState{};
        std::array<int, 349> m_keyStatePrevious{};
    };
} // namespace Input