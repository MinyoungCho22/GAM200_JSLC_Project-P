#pragma once
#include <array>

// GLFW/glfw3.h를 포함하는 대신 전방 선언을 사용합니다.
struct GLFWwindow;

// Input 네임스페이스 안에 키 enum과 클래스를 모두 정의합니다.
namespace Input
{
    enum Key {
        // 실제 GLFW 키 코드와 동일한 값
        Space = 32,
        A = 65,
        D = 68,
        E = 69,
        F = 70,
        S = 83,     // ✅ [추가] 메뉴용
        W = 87,     // ✅ [추가] 메뉴용
        Escape = 256,
        Enter = 257, // ✅ [추가] 메뉴용
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

        std::array<int, 349> m_keyState{};
        std::array<int, 349> m_keyStatePrevious{};
    };
} // namespace Input