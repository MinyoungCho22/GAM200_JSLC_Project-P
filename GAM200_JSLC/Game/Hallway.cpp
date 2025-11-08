// Hallway.cpp

#include "Hallway.hpp"
#include "Background.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"

// Hallway가 붙어있을 Room의 너비
constexpr float ROOM_WIDTH = 1920.0f;

void Hallway::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Hallway.png");

    m_size = { WIDTH, HEIGHT };

    // GameplayState::Draw()에 있던 위치 계산 로직사용
    // (Room 오른쪽 끝 + Hallway 너비의 절반)
    m_position = { ROOM_WIDTH + WIDTH / 2.0f, HEIGHT / 2.0f };
}

void Hallway::Update(double dt)
{
    // (void)dt; // 현재 비어있음
}

void Hallway::Draw(Shader& shader)
{
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);

    shader.setMat4("model", model);
    m_background->Draw(shader, model);
}

void Hallway::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }
}