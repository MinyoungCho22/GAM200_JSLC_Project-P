#pragma once
#include <array>

// GLFW/glfw3.h를 포함하는 대신 전방 선언을 사용
struct GLFWwindow;

// Input 네임스페이스 안에 키 enum과 클래스를 모두 정의
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

        bool IsKeyPressed(Key key) const;
        bool IsKeyTriggered(Key key) const;

    private:
        GLFWwindow* m_window = nullptr;

        // GLFW_KEY_LAST가 348이므로, 배열 크기는 349 (0~348)로 설정

        std::array<int, 349> m_keyState{};
        std::array<int, 349> m_keyStatePrevious{};
    };
} // namespace Input