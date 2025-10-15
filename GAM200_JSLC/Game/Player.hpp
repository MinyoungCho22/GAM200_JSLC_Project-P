#pragma once
#include "../Engine/Vec2.hpp"
#include "../Game/PurseCore.hpp"

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

    Math::Vec2 GetPosition() const { return position; }
    Math::Vec2 GetSize() const { return size; }
    PulseCore& GetPulseCore() { return m_pulseCore; }

private:
    // 기본 상태
    Math::Vec2 position;
    Math::Vec2 velocity;
    Math::Vec2 size;
    bool is_on_ground = false;
    int last_move_direction = 1;

    // [수정] 최대 펄스 100, 시작 펄스 20으로 설정
    PulseCore m_pulseCore{ 100.f, 20.f };

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