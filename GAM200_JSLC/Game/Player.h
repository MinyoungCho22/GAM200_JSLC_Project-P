#pragma once
#include "../Engine/Vec2.h"

class Shader;
struct GLFWwindow;

class Player
{
public:
    void Init(Vec2 startPos, const char* texturePath);
    void Update(double dt, GLFWwindow* p_window);
    void Draw(const Shader& shader) const;
    void Shutdown();

private:
    // 기본 상태
    Vec2 position;
    Vec2 velocity;
    Vec2 size;
    bool is_on_ground = false;
    int last_move_direction = 1; // 1: 오른쪽, -1: 왼쪽

    // 웅크리기 상태
    bool is_crouching = false;
    Vec2 original_size; // 원래 크기를 저장하기 위한 변수

    // 대시 상태
    bool is_dashing = false;
    float dash_timer = 0.0f;

    // 플레이어 능력치
    float move_speed = 300.0f;
    float jump_velocity = 500.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    // 렌더링 관련
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
};