#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PurseCore.hpp"      // [추가] PulseCore 헤더를 포함합니다.

class Shader;

class Player
{
public:
    void Init(Math::Vec2 startPos, const char* texturePath);
    void Update(double dt);
    void Draw(const Shader& shader) const;
    void Shutdown();

    // 행동 기반 함수들
    void MoveLeft();
    void MoveRight();
    void Jump();
    void Crouch();
    void StopCrouch();
    void Dash();

    // [추가] Collision Check를 위한 Get 함수들
    Math::Vec2 GetPosition() const { return position; }
    Math::Vec2 GetSize() const { return size; }

    // [추가] Player의 PulseCore에 접근할 수 있는 함수
    PulseCore& GetPulseCore() { return m_pulseCore; }

private:
    // 기본 상태
    Math::Vec2 position;
    Math::Vec2 velocity;
    Math::Vec2 size;
    bool is_on_ground = false;
    int last_move_direction = 1; // 1: 오른쪽, -1: 왼쪽

    // [추가] PulseCore 멤버 변수
    PulseCore m_pulseCore;

    // 웅크리기 상태
    bool is_crouching = false;
    Math::Vec2 original_size;

    // 대시 상태
    bool is_dashing = false;
    float dash_timer = 0.0f;

    // 플레이어 능력치
    float move_speed = 300.0f;
    float jump_velocity = 600.0f;
    float dash_speed = 900.0f;
    float dash_duration = 0.15f;

    // 렌더링 관련
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int textureID = 0;
};