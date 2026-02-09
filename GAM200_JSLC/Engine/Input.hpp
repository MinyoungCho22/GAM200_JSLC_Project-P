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
     * @enum MouseButton
     * @brief Mouse button codes mapped to GLFW standards.
     */
    enum class MouseButton {
        Left = 0,
        Right = 1,
        Middle = 2
    };

    /**
     * @class Input
     * @brief Manages keyboard and mouse input states, allowing for polling of held and triggered keys/buttons.
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
         * @brief Updates current and previous key/mouse states. Should be called once per frame.
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

        /**
         * @brief Checks if a mouse button is currently being held down.
         * @param button The mouse button to check.
         * @return True if the button is currently pressed.
         */
        bool IsMouseButtonPressed(MouseButton button) const;

        /**
         * @brief Checks if a mouse button was pressed exactly during this frame.
         * @param button The mouse button to check.
         * @return True only on the frame the button was first pressed.
         */
        bool IsMouseButtonTriggered(MouseButton button) const;

        /**
         * @brief Gets the current mouse position in screen coordinates.
         * @param x Reference to store the x coordinate.
         * @param y Reference to store the y coordinate.
         */
        void GetMousePosition(double& x, double& y) const;

        /**
         * @brief Gets the current mouse x position in screen coordinates.
         * @return The x coordinate.
         */
        double GetMouseX() const { return m_mouseX; }

        /**
         * @brief Gets the current mouse y position in screen coordinates.
         * @return The y coordinate.
         */
        double GetMouseY() const { return m_mouseY; }

    private:
        GLFWwindow* m_window = nullptr;

        // Buffers to store key states for the current and previous frames.
        // Used to detect edge triggers (Transition from Released to Pressed).
        std::array<int, 349> m_keyState{};
        std::array<int, 349> m_keyStatePrevious{};

        // Mouse button states (GLFW supports up to 8 buttons)
        std::array<int, 8> m_mouseButtonState{};
        std::array<int, 8> m_mouseButtonStatePrevious{};

        // Mouse position
        double m_mouseX = 0.0;
        double m_mouseY = 0.0;
    };
} // namespace Input