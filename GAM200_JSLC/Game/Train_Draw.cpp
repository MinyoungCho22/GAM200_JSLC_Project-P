// Train_Draw.cpp - All Draw* functions

#include "Train_Internal.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "MapObjectConfig.hpp"
#include "Robot.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// 솔리드 컬러로 채운 사각형을 지정 위치·크기로 그림
void Train::DrawFilledQuad(Shader& colorShader,
                             Math::Vec2 center, Math::Vec2 size,
                             float r, float g, float b, float a) const
{
    if (!m_skyVAO) return;

    Math::Matrix model = Math::Matrix::CreateTranslation(center) * Math::Matrix::CreateScale(size);
    colorShader.setMat4("model", model);
    colorShader.setVec3("objectColor", r, g, b);
    colorShader.setFloat("uAlpha", a);

    GL::BindVertexArray(m_skyVAO);
    GL::DrawArrays(GL_TRIANGLES, 0, 6);
    GL::BindVertexArray(0);
}
// ---------------------------------------------------------------------------
// 두께가 있는 타원 링을 위쪽 반원 형태로 그림 (사이렌 파동 이펙트에 사용함)
// ---------------------------------------------------------------------------
void Train::DrawCircleLine(Shader& colorShader,
    Math::Vec2 center, Math::Vec2 size,
    float thickness,
    float r, float g, float b, float a) const {
    
    constexpr int kSegments = 160;
    constexpr float kPi = 3.14159265359f;

    std::vector<float> vertices;
    vertices.reserve(kSegments * 2);

    const float rx = size.x * 0.5f;
    const float ry = size.y * 0.5f;

    const float halfThickness = thickness * 0.5f;

    const float outerRx = rx + halfThickness;
    const float outerRy = ry + halfThickness;
    const float innerRx = std::max(1.0f, rx - halfThickness);
    const float innerRy = std::max(1.0f, ry - halfThickness);


    for (int i = 0; i < kSegments; ++i) {
        const float progress = static_cast<float>(i) / static_cast<float>(kSegments - 1);
        const float angle = progress * kPi;
        const float cosA = std::cos(angle);
        const float sinA = std::sin(angle);

        // 바깥쪽 반원 점
        vertices.push_back(cosA * outerRx);
        vertices.push_back(sinA * outerRy);

        // 안쪽 반원 점
        vertices.push_back(cosA * innerRx);
        vertices.push_back(sinA * innerRy);
    }

    unsigned int vao = 0;
    unsigned int vbo = 0;

    GL::GenVertexArrays(1, &vao);
    GL::GenBuffers(1, &vbo);
    GL::BindVertexArray(vao);
    GL::BindBuffer(GL_ARRAY_BUFFER, vbo);
    GL::BufferData(GL_ARRAY_BUFFER,
                    vertices.size() * sizeof(float),
                    vertices.data(),
                    GL_DYNAMIC_DRAW);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
        2 * sizeof(float), static_cast<void*>(nullptr));

    Math::Matrix model = Math::Matrix::CreateTranslation(center);

    colorShader.setMat4("model", model);
    colorShader.setVec3("objectColor", r, g, b);
    colorShader.setFloat("uAlpha", a);
    
    GL::DrawArrays(GL_TRIANGLE_STRIP, 0, kSegments * 2);

    GL::BindBuffer(GL_ARRAY_BUFFER, 0);
    GL::BindVertexArray(0);

    GL::DeleteBuffers(1, &vbo);
    GL::DeleteVertexArrays(1, &vao);
}


// ---------------------------------------------------------------------------
// 석양 하늘 그라데이션과 레일 그림자를 카메라 시야 범위 내에서 그림 (텍스처 패스 이전에 호출함)
// ---------------------------------------------------------------------------
void Train::DrawBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    // Camera-visible interval (with safety margin) for dynamic repetition.
    // This keeps the draw count low while still preventing background seams.
    viewHalfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float drawMargin = 1400.0f;
    const float visibleLeft  = cameraPos.x - viewHalfW - drawMargin;
    const float visibleRight = cameraPos.x + viewHalfW + drawMargin;

    // When the camera follows the player upward (car 4 stacks), shift the sky vertically with the camera
    // so the sunset bands still fill the frame instead of leaving cleared black at the top.
    const float skyAnchorY = MIN_Y + HEIGHT * 0.5f;
    const float skyLift    = cameraPos.y - skyAnchorY;
    const auto  relY       = [&](float t) { return MIN_Y + HEIGHT * t + skyLift; };

    // Sky gradient only needs to cover current visible area.
    const float spanW = (visibleRight - visibleLeft) + 1200.0f;
    const float centerX = MIN_X + m_totalTrainWidth * 0.5f;
    const float camDx = cameraPos.x - centerX;
    // Keep base sky centered on camera so it never falls out of view.
    // Only foreground/background objects use parallax offsets.
    const float skyPx  = cameraPos.x;
    const float farPx  = centerX + camDx * 0.05f;
    const float midPx  = centerX + camDx * 0.16f;
    const float nearPx = centerX + camDx * 0.34f;

    // Smoother sunset gradient (many soft layers instead of hard 3 bands)
    DrawFilledQuad(colorShader, { skyPx, relY(0.90f) }, { spanW, HEIGHT * 0.22f }, 0.13f, 0.05f, 0.19f, 1.0f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.75f) }, { spanW, HEIGHT * 0.22f }, 0.22f, 0.08f, 0.20f, 0.95f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.60f) }, { spanW, HEIGHT * 0.20f }, 0.38f, 0.11f, 0.18f, 0.90f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.47f) }, { spanW, HEIGHT * 0.18f }, 0.58f, 0.17f, 0.14f, 0.88f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.36f) }, { spanW, HEIGHT * 0.16f }, 0.80f, 0.28f, 0.11f, 0.85f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.25f) }, { spanW, HEIGHT * 0.18f }, 0.53f, 0.18f, 0.10f, 0.70f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.11f) }, { spanW, HEIGHT * 0.22f }, 0.10f, 0.07f, 0.08f, 1.0f);

    // Sun + glow
    // Mild parallax for sun only (0.9x): keeps stability while adding depth.
    const float sunX = centerX + camDx * 0.9f + 320.0f;
    const float sunY = relY(0.37f);
    DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.34f, HEIGHT * 0.34f }, 1.00f, 0.48f, 0.18f, 0.28f);
    DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.18f, HEIGHT * 0.18f }, 1.00f, 0.62f, 0.24f, 0.58f);
    DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.09f, HEIGHT * 0.09f }, 1.00f, 0.79f, 0.35f, 0.95f);

    // Repeated cloud objects (dense, multi-row, infinite-style coverage)
    const float cloudBase = farPx;
    const float cloudStep = 620.0f;
    const int cloudMinI = static_cast<int>(std::floor((visibleLeft - cloudBase - 700.0f) / cloudStep));
    const int cloudMaxI = static_cast<int>(std::ceil((visibleRight - cloudBase + 700.0f) / cloudStep));
    for (int i = cloudMinI; i <= cloudMaxI; ++i)
    {
        const float x = cloudBase + i * 620.0f;
        const float y1 = relY(0.78f - 0.02f * static_cast<float>((i + 30) % 4));
        const float y2 = relY(0.66f - 0.02f * static_cast<float>((i + 11) % 5));
        const float y3 = relY(0.56f - 0.015f * static_cast<float>((i + 7) % 6));

        DrawFilledQuad(colorShader, { x,          y1 }, { 520.0f, 52.0f }, 0.40f, 0.17f, 0.27f, 0.26f);
        DrawFilledQuad(colorShader, { x + 120.0f, y1 - 24.0f }, { 360.0f, 38.0f }, 0.33f, 0.13f, 0.24f, 0.20f);

        DrawFilledQuad(colorShader, { x - 80.0f,  y2 }, { 430.0f, 42.0f }, 0.52f, 0.21f, 0.20f, 0.18f);
        DrawFilledQuad(colorShader, { x + 50.0f,  y2 - 20.0f }, { 300.0f, 30.0f }, 0.45f, 0.17f, 0.18f, 0.14f);

        DrawFilledQuad(colorShader, { x + 30.0f,  y3 }, { 340.0f, 28.0f }, 0.68f, 0.26f, 0.16f, 0.10f);
    }

    // Mid skyline (dynamic range from current camera visibility)
    const float midStep = 360.0f;
    const int midMinI = static_cast<int>(std::floor((visibleLeft - midPx - 300.0f) / midStep));
    const int midMaxI = static_cast<int>(std::ceil((visibleRight - midPx + 300.0f) / midStep));
    for (int i = midMinI; i <= midMaxI; ++i)
    {
        const float x = midPx + i * 360.0f;
        const float h = 110.0f + static_cast<float>((i + 60) % 7) * 26.0f;
        const float w = 130.0f + static_cast<float>((i + 60) % 4) * 22.0f;
        DrawFilledQuad(colorShader, { x, relY(0.13f) + h * 0.5f }, { w, h }, 0.10f, 0.06f, 0.09f, 0.95f);
    }

    // Near dark silhouette strip (foreground city/yard)
    const float nearStep = 210.0f;
    const int nearMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 250.0f) / nearStep));
    const int nearMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 250.0f) / nearStep));
    for (int i = nearMinI; i <= nearMaxI; ++i)
    {
        const float x = nearPx + i * 210.0f;
        const float h = 86.0f + static_cast<float>((i + 100) % 5) * 20.0f;
        DrawFilledQuad(colorShader, { x, relY(0.07f) + h * 0.5f }, { 150.0f, h }, 0.07f, 0.05f, 0.06f, 1.0f);
    }

    // Poles / masts
    const float poleStep = 160.0f;
    const int poleMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 120.0f) / poleStep));
    const int poleMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 120.0f) / poleStep));
    for (int i = poleMinI; i <= poleMaxI; ++i)
    {
        const float x = nearPx + i * 160.0f;
        DrawFilledQuad(colorShader, { x, relY(0.22f) },
                       { 10.0f, 170.0f + static_cast<float>((i + 80) % 3) * 36.0f },
                       0.06f, 0.04f, 0.05f, 0.94f);
    }
}

// ---------------------------------------------------------------------------
// 레일 타일을 수평으로 반복해 카메라 가시 범위만큼 그림
// ---------------------------------------------------------------------------
void Train::DrawRailTrack(Shader& shader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_railTile || m_railTileW <= 0.0f)
        return;

    const float safeHalfW  = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float railMargin = 600.0f;
    const float leftX      = cameraPos.x - safeHalfW - railMargin;
    const float rightX     = cameraPos.x + safeHalfW + railMargin;
    const float tileLeft0  = MIN_X;
    const int minTile = static_cast<int>(std::floor((leftX - tileLeft0) / m_railTileW)) - 1;
    const int maxTile = static_cast<int>(std::ceil((rightX - tileLeft0) / m_railTileW)) + 1;
    const float cy = MIN_Y + m_railTileH * 0.5f;

    for (int i = minTile; i <= maxTile; ++i)
    {
        float cx = MIN_X + i * m_railTileW + m_railTileW * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_railTileW, m_railTileH });
        m_railTile->Draw(shader, model);
    }
}

// 열차 칸 스프라이트(Car1~5), 밸브, 로봇을 카메라 시야 내에서 그림
void Train::Draw(Shader& shader, Math::Vec2 cameraPos, float viewHalfW) const
{
    // ── Train car images (move with trainOffset) ───────────────────────────
    const float trainLeft = MIN_X + m_trainOffset;

    if (m_firstTrain)
    {
        float cx = trainLeft + m_car1Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car1Width, HEIGHT });
        m_firstTrain->Draw(shader, model);
    }

    if (m_secondTrain)
    {
        float cx = trainLeft + m_car1Width + m_car2Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car2Width, HEIGHT });
        m_secondTrain->Draw(shader, model);
    }

    if (m_thirdTrain)
    {
        float cx = trainLeft + m_car1Width + m_car2Width + m_car3Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car3Width, HEIGHT });
        m_thirdTrain->Draw(shader, model);
    }

    if (m_thirdThirdTrain)
    {
        float cx = trainLeft + m_car1Width + m_car2Width + m_car3Width + m_car4Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car4Width, HEIGHT });
        m_thirdThirdTrain->Draw(shader, model);
    }

    if (m_fourthTrain)
    {
        float cx = trainLeft + m_car1Width + m_car2Width + m_car3Width + m_car4Width + m_car5Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car5Width, HEIGHT });
        m_fourthTrain->Draw(shader, model);
    }

    if (m_valveSprite && m_valveSprite->GetWidth() > 0)
    {
        const Math::Vec2 valveWorld = { trainLeft + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
        const float cwDeg = -m_valveOpenT * 115.0f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation(valveWorld) *
            Math::Matrix::CreateRotation(cwDeg) *
            Math::Matrix::CreateScale(m_valveVisualSize);
        m_valveSprite->Draw(shader, model);
    }

    // ── Robots (none currently, kept for future use) ─────────────────────
    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
            robot.Draw(shader);
    }
}


// ---------------------------------------------------------------------------
// 전투·사이렌·자동차 운반 드론 스프라이트를 모두 그림
// ---------------------------------------------------------------------------
void Train::DrawDrones(Shader& shader) const
{
    if (m_droneManager)
        m_droneManager->Draw(shader);
    if (m_carTransportDroneManager)
        m_carTransportDroneManager->Draw(shader);
    if (m_sirenDroneManager)
        m_sirenDroneManager->Draw(shader);
}

// 전투·사이렌·자동차 운반 드론의 레이더 범위 원을 그림
void Train::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_droneManager)
        m_droneManager->DrawRadars(colorShader, debugRenderer);
    if (m_carTransportDroneManager)
        m_carTransportDroneManager->DrawRadars(colorShader, debugRenderer);
    if (m_sirenDroneManager)
        m_sirenDroneManager->DrawRadars(colorShader, debugRenderer);
}

// 드론 체력 게이지, 로봇 체력 게이지 등을 그림
void Train::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_droneManager)
        m_droneManager->DrawGauges(colorShader, debugRenderer);
    if (m_carTransportDroneManager)
        m_carTransportDroneManager->DrawGauges(colorShader, debugRenderer);
    if (m_sirenDroneManager)
        m_sirenDroneManager->DrawGauges(colorShader, debugRenderer);
    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
            robot.DrawGauge(colorShader, debugRenderer);
    }
}


// ---------------------------------------------------------------------------
// 디버그 모드에서 히트박스·히딩 스팟·레일 발판을 색상 박스로 그림
// ---------------------------------------------------------------------------
void Train::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Pulse source boxes (green)
    for (const auto& source : m_pulseSources)
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 0.0f, 1.0f });

    // Config obstacle boxes (red)
    for (const auto& obs : m_obstacles)
        debugRenderer.DrawBox(colorShader, obs.pos, obs.size, { 1.0f, 0.0f });

    // 월드 고정 레일 발판 (orange)
    for (const auto& hb : m_staticWorldHitboxes)
    {
        const Math::Vec2 worldPos = { MIN_X + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        debugRenderer.DrawBox(colorShader, worldPos, hb.size, 1.0f, 0.55f, 0.2f);
    }

    // Hiding spots (green) — move with train, same as Hallway display convention
    {
        const float trainLeft = MIN_X + m_trainOffset;
        for (const auto& spot : m_hidingSpots)
        {
            Math::Vec2 worldPos = { trainLeft + spot.localCenter.x, MIN_Y + spot.localCenter.y };
            debugRenderer.DrawBox(colorShader, worldPos, spot.size, { 0.3f, 1.0f });
        }
    }

    // Train hitboxes (cyan) — shrink display by 2px so stacked box edges don't merge
    const float trainLeft = MIN_X + m_trainOffset;
    constexpr float kDebugShrink = 2.0f;
    for (const auto& hb : m_trainHitboxes)
    {
        Math::Vec2 worldPos = { trainLeft + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        Math::Vec2 displaySize = { hb.size.x - kDebugShrink, hb.size.y - kDebugShrink };
        debugRenderer.DrawBox(colorShader, worldPos, displaySize, 0.0f, 1.0f, 1.0f);
    }

    // Map boundary markers (white)
    const float bndH = HEIGHT;
    debugRenderer.DrawBox(colorShader,
        { MIN_X,         MIN_Y + bndH * 0.5f }, { 10.0f, bndH }, { 1.0f, 1.0f });
    debugRenderer.DrawBox(colorShader,
        { MIN_X + m_totalTrainWidth, MIN_Y + bndH * 0.5f }, { 10.0f, bndH }, { 1.0f, 1.0f });
}
