#include "Input.hpp"
#include <GLFW/glfw3.h> // Input.cpp만 GLFW 헤더를 포함하도록 하자

// 모든 함수 구현을 Input 네임스페이스 안에 두어야 함
namespace Input
{
    void Input::Initialize(GLFWwindow* window)
    {
        m_window = window;
        // GLFW_KEY_LAST가 348이므로, 349 크기 배열을 사용
        m_keyState.fill(GLFW_RELEASE);
        m_keyStatePrevious.fill(GLFW_RELEASE);
    }

    void Input::Update()
    {
        m_keyStatePrevious = m_keyState;

        // 0부터 348 (GLFW_KEY_LAST)까지 모든 키의 상태를 폴링
        for (int key = 0; key <= GLFW_KEY_LAST; ++key)
        {
            m_keyState[key] = glfwGetKey(m_window, key);
        }
    }

    bool Input::IsKeyPressed(Key key) const
    {
        // Input::Key enum의 정수 값이 GLFW 키 코드와 동일하므로
        // 배열의 인덱스로 바로 사용할 수 있음
        return m_keyState.at(key) == GLFW_PRESS;
    }

    bool Input::IsKeyTriggered(Key key) const
    {
        return (m_keyState.at(key) == GLFW_PRESS && m_keyStatePrevious.at(key) == GLFW_RELEASE);
    }

} // namespace Input