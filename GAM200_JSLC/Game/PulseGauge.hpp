#pragma once
#include "../Engine/Vec2.hpp"

class Shader;

class PulseGauge
{
public:
    // ������ ���� ��ġ, ũ�⸦ �����ϰ� ������ ���ҽ��� �ʱ�ȭ�մϴ�.
    void Initialize(Math::Vec2 position, Math::Vec2 size);

    // ���� �޽� ���� �ִ� �޽� ���� �޾� ���� ���¸� ������Ʈ�մϴ�.
    void Update(float current_pulse, float max_pulse);

    // ������ �ٸ� ȭ�鿡 �׸��ϴ�.
    void Draw(Shader& shader);

    // ����� ���ҽ��� �����մϴ�.
    void Shutdown();

private:
    Math::Vec2 m_position; // ������ ���� ��ġ (�߽���)
    Math::Vec2 m_size;     // ������ ���� ��ü ũ��
    float m_pulse_ratio = 1.0f; // �޽� ���� (0.0 ~ 1.0)

    // ��������
    unsigned int VAO = 0;
    unsigned int VBO = 0;
};