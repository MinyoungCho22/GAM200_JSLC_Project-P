#include "Player.h"
#include "../Engine/Vec2.h" // Vec2 클래스를 사용하기 위해 포함
#include <glad/glad.h>
#include <GLFW/glfw3.h> // 입력을 위해 포함

// 물리 상수 정의
const float GRAVITY = -980.0f;
const float MOVE_SPEED = 300.0f;
const float JUMP_VELOCITY = 500.0f;

// Init 함수의 매개변수 타입을 Vec2로 변경
void Player::Init(Vec2 startPos)
{
    position = startPos;
    // Vec2 생성자를 사용하여 초기화
    velocity = Vec2(0.0f, 0.0f);
    size = Vec2(50.0f, 80.0f);
}

void Player::Update(double dt, GLFWwindow* p_window)
{
    // 1. 좌/우 입력 처리
    velocity.x = 0; // 매 프레임 수평 속도 초기화
    if (glfwGetKey(p_window, GLFW_KEY_A) == GLFW_PRESS)
    {
        velocity.x -= MOVE_SPEED;
    }
    if (glfwGetKey(p_window, GLFW_KEY_D) == GLFW_PRESS)
    {
        velocity.x += MOVE_SPEED;
    }

    // 2. 점프 입력 처리 (기획서에 맞춰 Space 키로 유지)
    // // <- 이 부분을 주석 처리
    if (is_on_ground && glfwGetKey(p_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        // [cite] // <- 이 부분을 주석 처리
    {
        velocity.y = JUMP_VELOCITY;
        // E0169: 선언이 필요합니다.  <-- 이 줄은 삭제하거나 주석 처리
        is_on_ground = false;
    }

    // 기획서에 있는 S키를 이용한 숨기/앉기 기능 추가 (기본 틀)
    // // <- 이 부분을 주석 처리
    if (is_on_ground && glfwGetKey(p_window, GLFW_KEY_S) == GLFW_PRESS)
        // [cite] // <- 이 부분을 주석 처리
    {
        // 여기에 앉기 로직 추가 (예: 플레이어 크기 변경)
    }

    // 3. 물리 계산 (중력 적용)
    velocity.y += GRAVITY * static_cast<float>(dt);

    // 4. 위치 업데이트 (델타 타임 적용)
    position += velocity * static_cast<float>(dt);

    // 5. 간단한 충돌 처리 (바닥)
    if (position.y < 100.0f)
    {
        position.y = 100.0f;
        velocity.y = 0;
        is_on_ground = true;
    }
}

void Player::Draw() const
{
    // 플레이어를 흰색 사각형으로 그리기 (임시)
    // 참고: glBegin/glEnd는 오래된 방식이며,
    // 추후 VBO, VAO, Shader를 사용하는 방식으로 변경해야 합니다.
    glBegin(GL_QUADS);
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(position.x, position.y);
    glVertex2f(position.x + size.x, position.y);
    glVertex2f(position.x + size.x, position.y + size.y);
    glVertex2f(position.x, position.y + size.y);
    glEnd();

    // 바닥을 회색 선으로 그리기 (임시)
    glBegin(GL_LINES);
    glColor3f(0.5f, 0.5f, 0.5f);
    glVertex2f(0.0f, 100.0f);
    glVertex2f(1280.0f, 100.0f);
    glEnd();
}