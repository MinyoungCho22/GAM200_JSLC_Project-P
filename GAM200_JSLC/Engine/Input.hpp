#pragma once
#include <array>

// GLFW/glfw3.h를 포함하는 대신 전방 선언을 사용합니다.
struct GLFWwindow;

// Input 네임스페이스 안에 키 enum과 클래스를 모두 정의합니다.
namespace Input
{
    enum Key {
        Space = 32,
        A = 65,
        D = 68,
        E = 69,
        F = 70,
        S = 83,
        W = 87,
        Escape = 256,
        Tab = 258,
        LeftShift = 340
    };

    class Input
    {
    public:
        Input() = default;
        void Initialize(GLFWwindow* window);
        void Update();

        // 클래스가 같은 네임스페이스 안에 있으므로, "Key"만 사용하면 됩니다.
        bool IsKeyPressed(Key key) const;
        bool IsKeyTriggered(Key key) const;

    private:
        GLFWwindow* m_window = nullptr;

        // GLFW_KEY_LAST가 348이므로, 배열 크기는 349 (0~348)로 설정합니다.
        std::array<int, 349> m_keyState;
        std::array<int, 349> m_keyStatePrevious;
    };
} // namespace Input