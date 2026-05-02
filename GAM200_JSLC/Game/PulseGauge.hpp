//PulseGauge.hpp

#pragma once
#include "../Engine/Vec2.hpp"
#include "../Engine/Input.hpp" 
#include "Background.hpp"
#include <memory>

class Shader;

class PulseGauge
{
public:

    void Initialize();
    void Update(float current_pulse, float max_pulse);
    void Draw(Shader& shader);
    void Shutdown();

private:
    std::unique_ptr<Background> m_frame;
    std::unique_ptr<Background> m_fillCell;
    std::unique_ptr<Background> m_emptyCell;

    std::unique_ptr<Background> m_dialFrame;
    std::unique_ptr<Background> m_dialNeedle;
    std::unique_ptr<Background> m_dialCenterCap;

    Math::Vec2 m_position = { 60.0f, 520.0f };
    Math::Vec2 m_size = { 89.0f, 360.0f };

    Math::Vec2 m_cellSize = { 53.0f, 19.0f };
    Math::Vec2 m_cellStartPosition;

    float m_cellGapY = 28.0f;
    int m_totalCells = 10;
    float m_pulse_ratio = 0.0f;

    Math::Vec2 m_dialPosition = { 65.5f, 755.5f };
    Math::Vec2 m_dialSize = { 81.0f, 81.0f };

    float m_needleMinAngle = 0.0f;
    float m_needleMaxAngle = -180.0f;
};