// PulseGauge.cpp

#include "PulseGauge.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Engine.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"

void PulseGauge::Initialize()
{
    m_frame = std::make_unique<Background>();
    m_frame->Initialize("Asset/pulse/PulseBar_FrameWithEmptyCells.png");

    m_fillCell = std::make_unique<Background>();
    m_fillCell->Initialize("Asset/pulse/PulseBar_Cell_Fill.png");

    m_emptyCell = std::make_unique<Background>();
    m_emptyCell->Initialize("Asset/pulse/PulseBar_Cell_Empty.png");

    m_dialFrame = std::make_unique<Background>();
    m_dialFrame->Initialize("Asset/pulse/PulseDial_Frame.png");

    m_dialNeedle = std::make_unique<Background>();
    m_dialNeedle->Initialize("Asset/pulse/PulseDial_Needle.png");

    m_dialCenterCap = std::make_unique<Background>();
    m_dialCenterCap->Initialize("Asset/pulse/PulseDial_CenterCap.png");

    m_cellStartPosition = {
    m_position.x + 5.0f,
    m_position.y - 110.0f
    };
}

void PulseGauge::Update(float current_pulse, float max_pulse)
{
    if (max_pulse > 0)
    {
        m_pulse_ratio = current_pulse / max_pulse;
    }
    else
    {
        m_pulse_ratio = 0.0f;
    }
}

void PulseGauge::Draw(Shader& shader)
{
    int filledCells = static_cast<int>(m_pulse_ratio * m_totalCells + 0.5f);

    if (filledCells < 0) filledCells = 0;
    if (filledCells > m_totalCells) filledCells = m_totalCells;

    float r = 1.0f, g = 1.0f, b = 1.0f;
    float tintStrength = 0.6f;

    if (m_pulse_ratio < 0.2f)
    {
        r = 1.0f; g = 0.15f; b = 0.15f;
    }
    else if (m_pulse_ratio < 0.5f)
    {
        r = 1.0f; g = 0.6f; b = 0.1f;
    }
    else if (m_pulse_ratio < 0.7f)
    {
        r = 0.8f; g = 0.25f; b = 1.0f;
    }
    else
    {
        r = 0.25f; g = 1.0f; b = 0.45f;
    }

    // 1. bar frame (tint X)
    Math::Matrix frameModel =
        Math::Matrix::CreateTranslation(m_position) *
        Math::Matrix::CreateScale(m_size);

    shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    shader.setFloat("tintStrength", 0.0f);
    m_frame->Draw(shader, frameModel);

    // 2. bar cells (tint X)
    shader.setFloat("tintStrength", 0.0f);

    for (int i = 0; i < m_totalCells; ++i)
    {
        Math::Vec2 cellPos = {
            m_cellStartPosition.x,
            m_cellStartPosition.y + i * m_cellGapY
        };

        Math::Matrix cellModel =
            Math::Matrix::CreateTranslation(cellPos) *
            Math::Matrix::CreateScale(m_cellSize);

        m_emptyCell->Draw(shader, cellModel);
    }

    for (int i = 0; i < filledCells; ++i)
    {
        Math::Vec2 cellPos = {
            m_cellStartPosition.x,
            m_cellStartPosition.y + i * m_cellGapY
        };

        Math::Matrix cellModel =
            Math::Matrix::CreateTranslation(cellPos) *
            Math::Matrix::CreateScale(m_cellSize);

        m_fillCell->Draw(shader, cellModel);
    }

    // 3. dialFrame + needle (tint O)
    shader.setVec3("colorTint", r, g, b);
    shader.setFloat("tintStrength", tintStrength);

    Math::Matrix dialFrameModel =
        Math::Matrix::CreateTranslation(m_dialPosition) *
        Math::Matrix::CreateScale(m_dialSize);

    m_dialFrame->Draw(shader, dialFrameModel);

    float needleRatio = m_pulse_ratio;
    if (needleRatio < 0.0f) needleRatio = 0.0f;
    if (needleRatio > 1.0f) needleRatio = 1.0f;

    float needleAngle = -180.0f * needleRatio;

    Math::Matrix needleModel =
        Math::Matrix::CreateTranslation(m_dialPosition) *
        Math::Matrix::CreateRotation(needleAngle) *
        Math::Matrix::CreateScale(m_dialSize);

    m_dialNeedle->Draw(shader, needleModel);

    // 4. centerCap (tint X)
    shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    shader.setFloat("tintStrength", 0.0f);

    Math::Matrix centerCapModel =
        Math::Matrix::CreateTranslation(m_dialPosition) *
        Math::Matrix::CreateScale(m_dialSize);

    m_dialCenterCap->Draw(shader, centerCapModel);
}

void PulseGauge::Shutdown()
{
    if (m_frame)
        m_frame->Shutdown();

    if (m_emptyCell)
        m_emptyCell->Shutdown();

    if (m_fillCell)
        m_fillCell->Shutdown();

    if (m_dialFrame)
        m_dialFrame->Shutdown();

    if (m_dialNeedle)
        m_dialNeedle->Shutdown();

    if (m_dialCenterCap)
        m_dialCenterCap->Shutdown();
}