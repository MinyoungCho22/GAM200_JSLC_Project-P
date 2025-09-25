#include "Player.h"
#include "../Engine/Vec2.h" // Vec2 Ŭ������ ����ϱ� ���� ����
#include <glad/glad.h>
#include <GLFW/glfw3.h> // �Է��� ���� ����

// ���� ��� ����
const float GRAVITY = -980.0f;
const float MOVE_SPEED = 300.0f;
const float JUMP_VELOCITY = 500.0f;

// Init �Լ��� �Ű����� Ÿ���� Vec2�� ����
void Player::Init(Vec2 startPos)
{
    position = startPos;
    // Vec2 �����ڸ� ����Ͽ� �ʱ�ȭ
    velocity = Vec2(0.0f, 0.0f);
    size = Vec2(50.0f, 80.0f);
}

void Player::Update(double dt, GLFWwindow* p_window)
{
    // 1. ��/�� �Է� ó��
    velocity.x = 0; // �� ������ ���� �ӵ� �ʱ�ȭ
    if (glfwGetKey(p_window, GLFW_KEY_A) == GLFW_PRESS)
    {
        velocity.x -= MOVE_SPEED;
    }
    if (glfwGetKey(p_window, GLFW_KEY_D) == GLFW_PRESS)
    {
        velocity.x += MOVE_SPEED;
    }

    // 2. ���� �Է� ó�� (��ȹ���� ���� Space Ű�� ����)
    // // <- �� �κ��� �ּ� ó��
    if (is_on_ground && glfwGetKey(p_window, GLFW_KEY_SPACE) == GLFW_PRESS)
        // [cite] // <- �� �κ��� �ּ� ó��
    {
        velocity.y = JUMP_VELOCITY;
        // E0169: ������ �ʿ��մϴ�.  <-- �� ���� �����ϰų� �ּ� ó��
        is_on_ground = false;
    }

    // ��ȹ���� �ִ� SŰ�� �̿��� ����/�ɱ� ��� �߰� (�⺻ Ʋ)
    // // <- �� �κ��� �ּ� ó��
    if (is_on_ground && glfwGetKey(p_window, GLFW_KEY_S) == GLFW_PRESS)
        // [cite] // <- �� �κ��� �ּ� ó��
    {
        // ���⿡ �ɱ� ���� �߰� (��: �÷��̾� ũ�� ����)
    }

    // 3. ���� ��� (�߷� ����)
    velocity.y += GRAVITY * static_cast<float>(dt);

    // 4. ��ġ ������Ʈ (��Ÿ Ÿ�� ����)
    position += velocity * static_cast<float>(dt);

    // 5. ������ �浹 ó�� (�ٴ�)
    if (position.y < 100.0f)
    {
        position.y = 100.0f;
        velocity.y = 0;
        is_on_ground = true;
    }
}

void Player::Draw() const
{
    // �÷��̾ ��� �簢������ �׸��� (�ӽ�)
    // ����: glBegin/glEnd�� ������ ����̸�,
    // ���� VBO, VAO, Shader�� ����ϴ� ������� �����ؾ� �մϴ�.
    glBegin(GL_QUADS);
    glColor3f(1.0f, 1.0f, 1.0f);
    glVertex2f(position.x, position.y);
    glVertex2f(position.x + size.x, position.y);
    glVertex2f(position.x + size.x, position.y + size.y);
    glVertex2f(position.x, position.y + size.y);
    glEnd();

    // �ٴ��� ȸ�� ������ �׸��� (�ӽ�)
    glBegin(GL_LINES);
    glColor3f(0.5f, 0.5f, 0.5f);
    glVertex2f(0.0f, 100.0f);
    glVertex2f(1280.0f, 100.0f);
    glEnd();
}