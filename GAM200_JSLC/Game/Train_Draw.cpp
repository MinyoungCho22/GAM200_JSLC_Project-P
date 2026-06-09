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

// ---------------------------------------------------------------------------
// [DrawFilledQuad]
// - 기능: 단색(Solid Color)으로 채워진 직사각형(Quad)을 지정된 위치와 크기로 렌더링합니다.
// - 매개변수:
//   - colorShader: 렌더링에 사용할 단색 셰이더 레퍼런스
//   - center: 화면 월드 상의 직사각형 중심 위치
//   - size: 직사각형의 가로/세로 크기
//   - r, g, b, a: 색상 값 (RGB 및 알파값)
// - 가이드라인: 하늘 그라데이션, 암막 오버레이 등 단순 기하도형 표현 시 사용하십시오.
// ---------------------------------------------------------------------------
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
// [DrawCircleLine]
// - 기능: 사이렌 음파 이펙트 등 반원 형태의 두께를 가진 링 라인을 렌더링합니다.
// - 매개변수:
//   - colorShader: 렌더링에 사용할 단색 셰이더 레퍼런스
//   - center: 반원의 중심 위치
//   - size: 반원의 가로/세로 반지름 스케일
//   - thickness: 라인의 선 두께
//   - r, g, b, a: 색상 및 투명도
// - 가이드라인: 삼각함수를 통해 정점을 동적으로 빌드하여 그립니다. 대량 호출 시 성능 저하 주의 필요.
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
// [DrawBackground]
// - 기능: 석양 하늘 그라데이션, 태양광 및 패럴랙스 구름 실루엣을 시야 범위 내에 렌더링합니다.
// - 매개변수:
//   - colorShader: 단색 채우기 셰이더 레퍼런스
//   - cameraPos: 현재 카메라의 월드 위치 (패럴랙스 보정용)
//   - viewHalfW: 화면 가로 반폭 (가시 범위 검사용)
// - 가이드라인: 터널 진입 시 `m_car3TunnelInsideViewActive` 분기를 거치며, 어두운 암전 백드롭으로 대체됩니다.
// ---------------------------------------------------------------------------
void Train::DrawBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (m_car3TunnelInsideViewActive)
    {
        viewHalfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
        const float spanW = m_tunnelInsideWorldWidth + 1200.f;
        DrawFilledQuad(colorShader, cameraPos, { spanW, HEIGHT + 1800.f }, 0.04f, 0.04f, 0.05f, 1.0f);
        return;
    }

    const float tunnelBlend = GetEffectiveTunnelBlend(cameraPos);
    const float sunsetMul   = 1.0f - tunnelBlend;
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

    if (sunsetMul > 0.001f)
    {
        // Smoother sunset gradient (many soft layers instead of hard 3 bands)
        DrawFilledQuad(colorShader, { skyPx, relY(0.90f) }, { spanW, HEIGHT * 0.22f }, 0.13f, 0.05f, 0.19f, 1.0f * sunsetMul);
        DrawFilledQuad(colorShader, { skyPx, relY(0.75f) }, { spanW, HEIGHT * 0.22f }, 0.22f, 0.08f, 0.20f, 0.95f * sunsetMul);
        DrawFilledQuad(colorShader, { skyPx, relY(0.60f) }, { spanW, HEIGHT * 0.20f }, 0.38f, 0.11f, 0.18f, 0.90f * sunsetMul);
        DrawFilledQuad(colorShader, { skyPx, relY(0.47f) }, { spanW, HEIGHT * 0.18f }, 0.58f, 0.17f, 0.14f, 0.88f * sunsetMul);
        DrawFilledQuad(colorShader, { skyPx, relY(0.36f) }, { spanW, HEIGHT * 0.16f }, 0.80f, 0.28f, 0.11f, 0.85f * sunsetMul);
        DrawFilledQuad(colorShader, { skyPx, relY(0.25f) }, { spanW, HEIGHT * 0.18f }, 0.53f, 0.18f, 0.10f, 0.70f * sunsetMul);
        DrawFilledQuad(colorShader, { skyPx, relY(0.11f) }, { spanW, HEIGHT * 0.22f }, 0.10f, 0.07f, 0.08f, 1.0f * sunsetMul);

        // Sun + glow
        // Mild parallax for sun only (0.9x): keeps stability while adding depth.
        const float sunX = centerX + camDx * 0.9f + 320.0f;
        const float sunY = relY(0.37f);
        DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.34f, HEIGHT * 0.34f }, 1.00f, 0.48f, 0.18f, 0.28f * sunsetMul);
        DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.18f, HEIGHT * 0.18f }, 1.00f, 0.62f, 0.24f, 0.58f * sunsetMul);
        DrawFilledQuad(colorShader, { sunX, sunY }, { HEIGHT * 0.09f, HEIGHT * 0.09f }, 1.00f, 0.79f, 0.35f, 0.95f * sunsetMul);

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

            DrawFilledQuad(colorShader, { x,          y1 }, { 520.0f, 52.0f }, 0.40f, 0.17f, 0.27f, 0.26f * sunsetMul);
            DrawFilledQuad(colorShader, { x + 120.0f, y1 - 24.0f }, { 360.0f, 38.0f }, 0.33f, 0.13f, 0.24f, 0.20f * sunsetMul);

            DrawFilledQuad(colorShader, { x - 80.0f,  y2 }, { 430.0f, 42.0f }, 0.52f, 0.21f, 0.20f, 0.18f * sunsetMul);
            DrawFilledQuad(colorShader, { x + 50.0f,  y2 - 20.0f }, { 300.0f, 30.0f }, 0.45f, 0.17f, 0.18f, 0.14f * sunsetMul);

            DrawFilledQuad(colorShader, { x + 30.0f,  y3 }, { 340.0f, 28.0f }, 0.68f, 0.26f, 0.16f, 0.10f * sunsetMul);
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
            DrawFilledQuad(colorShader, { x, relY(0.13f) + h * 0.5f }, { w, h }, 0.10f, 0.06f, 0.09f, 0.95f * sunsetMul);
        }

        // Near dark silhouette strip (foreground city/yard)
        const float nearStep = 210.0f;
        const int nearMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 250.0f) / nearStep));
        const int nearMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 250.0f) / nearStep));
        for (int i = nearMinI; i <= nearMaxI; ++i)
        {
            const float x = nearPx + i * 210.0f;
            const float h = 86.0f + static_cast<float>((i + 100) % 5) * 20.0f;
            DrawFilledQuad(colorShader, { x, relY(0.07f) + h * 0.5f }, { 150.0f, h }, 0.07f, 0.05f, 0.06f, 1.0f * sunsetMul);
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
                           0.06f, 0.04f, 0.05f, 0.94f * sunsetMul);
        }
    }

    if (tunnelBlend > 0.001f)
    {
        const float tunnelA = tunnelBlend;
        DrawFilledQuad(colorShader, { skyPx, relY(0.52f) }, { spanW, HEIGHT * 1.15f }, 0.03f, 0.03f, 0.04f, tunnelA);
        DrawFilledQuad(colorShader, { skyPx, relY(0.30f) }, { spanW, HEIGHT * 0.70f }, 0.05f, 0.05f, 0.06f, tunnelA);
    }
}

// ---------------------------------------------------------------------------
// [DrawRailTrack]
// - 기능: 기차가 달리는 하단 레일 타일을 수평으로 가시 범위만큼 반복 렌더링합니다.
// - 매개변수:
//   - shader: 텍스처 렌더링에 사용할 스프라이트 셰이더 레퍼런스
//   - cameraPos: 현재 카메라 월드 좌표
//   - viewHalfW: 화면 가로 반폭
// - 가이드라인: 터널 내부 뷰(`m_car3TunnelInsideViewActive`)에서는 레일 타일을 그리지 않고 스킵합니다.
// ---------------------------------------------------------------------------
void Train::DrawRailTrack(Shader& shader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (m_car3TunnelInsideViewActive || m_car3TunnelInsideTransitionActive)
        return;
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
// ---------------------------------------------------------------------------
// [Draw]
// - 기능: 열차 차량 스프라이트(1~5칸), 하이딩 스팟, 사이렌 장치, 파이프(WaterOpener) 및 밸브(Valve)를 렌더링합니다.
// - 매개변수:
//   - shader: 메인 텍스처를 렌더링할 셰이더 레퍼런스
//   - cameraPos: 현재 카메라 위치
//   - viewHalfW: 화면 가로 반폭
//   - outlineShader: 글로우 아웃라인 렌더링용 셰이더 포인터 (인터리브드 그리기용)
//   - playerPos: 플레이어 위치 (글로우 조건 판정용)
//   - projection: 월드 좌표 투영 행렬 포인터 (인터리브드 그리기용)
// - 가이드라인: 중첩 오브젝트(파이프, 밸브)는 깊이 문제 방지를 위해 순서대로 텍스처와 아웃라인을 교차 렌더링합니다.
// ---------------------------------------------------------------------------
void Train::Draw(Shader& shader, Math::Vec2 cameraPos, float viewHalfW,
                 Shader* outlineShader, Math::Vec2 playerPos,
                 const Math::Matrix* projection) const
{
    // 열차만 MainLayer — Turnel_Inside·오브젝트·플레이어는 ForegroundLayer
    if (m_car3TunnelInsideViewActive)
    {
        DrawTunnelInsideTrainForeground(shader);
        return;
    }

    auto getAABBProximitySq = [](Math::Vec2 pPos, Math::Vec2 boxPos, Math::Vec2 boxSize) {
        float dx = std::max(0.0f, std::abs(pPos.x - boxPos.x) - boxSize.x * 0.5f);
        float dy = std::max(0.0f, std::abs(pPos.y - boxPos.y) - boxSize.y * 0.5f);
        return dx * dx + dy * dy;
    };
    const float proxDist = 300.f;
    const float proxDistSq = proxDist * proxDist;

    // ── Train car images (move with trainOffset) ───────────────────────────
    const float trainLeft = MIN_X + m_trainOffset;
    const float tunnelBlend = GetEffectiveTunnelBlend(cameraPos);

    // 터널 에셋은 열차와 분리된 "배경"이므로 월드 고정으로 먼저 그린다.
    // 즉, 기차가 이동해도 함께 이동하지 않으며 열차 스프라이트보다 뒤에 유지된다.
    if (tunnelBlend > 0.001f && m_tunnelCeilTex && m_tunnelCeilTex->GetWidth() > 0)
    {
        const float tileW = static_cast<float>(m_tunnelCeilTex->GetWidth());
        const float tileH = static_cast<float>(m_tunnelCeilTex->GetHeight());
        const float leftX = cameraPos.x - viewHalfW - 600.f;
        const float rightX = cameraPos.x + viewHalfW + 600.f;
        const int startI = static_cast<int>(std::floor((leftX - MIN_X) / std::max(tileW, 1.f))) - 1;
        const int endI = static_cast<int>(std::ceil((rightX - MIN_X) / std::max(tileW, 1.f))) + 1;
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        shader.setFloat("tintStrength", 0.0f);
        shader.setFloat("alpha", tunnelBlend);
        for (int i = startI; i <= endI; ++i)
        {
            const float cx = MIN_X + i * tileW + tileW * 0.5f;
            Math::Matrix ceilModel =
                Math::Matrix::CreateTranslation({ cx, MIN_Y + HEIGHT - tileH * 0.5f + 12.f })
                * Math::Matrix::CreateScale({ tileW, tileH });
            m_tunnelCeilTex->Draw(shader, ceilModel);
        }
    }

    // Turnel_Front: 월드 고정(열차 m_trainOffset 미적용). 원색 alpha=1.0 — tunnelBlend 알파는 회색처럼 보이게 함.
    if (tunnelBlend > 0.001f && m_tunnelFrontTex && m_tunnelFrontTex->GetWidth() > 0)
    {
        const float extStaticLeft = MIN_X + m_car1Width + m_car2Width + m_car3Width;
        const float pW = static_cast<float>(m_tunnelFrontTex->GetWidth());
        const float pH = static_cast<float>(m_tunnelFrontTex->GetHeight());
        shader.setFloat("alpha", 1.0f);
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        shader.setFloat("tintStrength", 0.0f);
        const float xA = extStaticLeft + 45.f;
        const float xB = extStaticLeft + m_car3ExtensionWidths[0] + 20.f;
        const float xC = extStaticLeft + m_car3ExtensionWidths[0] + m_car3ExtensionWidths[1] + 10.f;
        const float py = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix mA = Math::Matrix::CreateTranslation({ xA, py }) * Math::Matrix::CreateScale({ pW, pH });
        Math::Matrix mB = Math::Matrix::CreateTranslation({ xB, py }) * Math::Matrix::CreateScale({ pW, pH });
        Math::Matrix mC = Math::Matrix::CreateTranslation({ xC, py }) * Math::Matrix::CreateScale({ pW, pH });
        m_tunnelFrontTex->Draw(shader, mA);
        m_tunnelFrontTex->Draw(shader, mB);
        m_tunnelFrontTex->Draw(shader, mC);
    }

    // Turnel_Back: 월드 고정, 각 터널 세그먼트의 출구 아치를 그린다.
    if (tunnelBlend > 0.001f && m_tunnelBackTex && m_tunnelBackTex->GetWidth() > 0)
    {
        const float extStaticLeft = MIN_X + m_car1Width + m_car2Width + m_car3Width;
        const float pW = static_cast<float>(m_tunnelBackTex->GetWidth());
        const float pH = static_cast<float>(m_tunnelBackTex->GetHeight());
        shader.setFloat("alpha", 1.0f);
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        shader.setFloat("tintStrength", 0.0f);
        const float xA = extStaticLeft + m_car3ExtensionWidths[0] - 45.f;
        const float xB = extStaticLeft + m_car3ExtensionWidths[0] + m_car3ExtensionWidths[1] - 20.f;
        const float xC = extStaticLeft + m_car3ExtensionWidths[0] + m_car3ExtensionWidths[1] + m_car3ExtensionWidths[2] - 10.f;
        const float py = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix mA = Math::Matrix::CreateTranslation({ xA, py }) * Math::Matrix::CreateScale({ pW, pH });
        Math::Matrix mB = Math::Matrix::CreateTranslation({ xB, py }) * Math::Matrix::CreateScale({ pW, pH });
        Math::Matrix mC = Math::Matrix::CreateTranslation({ xC, py }) * Math::Matrix::CreateScale({ pW, pH });
        m_tunnelBackTex->Draw(shader, mA);
        m_tunnelBackTex->Draw(shader, mB);
        m_tunnelBackTex->Draw(shader, mC);
    }

    shader.setFloat("alpha", 1.0f);
    shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    shader.setFloat("tintStrength", 0.0f);

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

    {
        float extLeft = trainLeft + m_car1Width + m_car2Width + m_car3Width;
        for (int i = 0; i < kCar3ExtensionCount; ++i)
        {
            Background* tex = m_car3ExtensionTrains[static_cast<size_t>(i)].get();
            if (m_car3InsideViewActive)
            {
                if (i == 0 && m_car3InsideTrainA && m_car3InsideTrainA->GetWidth() > 0)
                    tex = m_car3InsideTrainA.get();
                else if (i == 1 && m_car3InsideTrainB && m_car3InsideTrainB->GetWidth() > 0)
                    tex = m_car3InsideTrainB.get();
            }
            if (!tex)
                continue;
            const float w = m_car3ExtensionWidths[static_cast<size_t>(i)];
            const float cx = extLeft + w * 0.5f;
            const float cy = MIN_Y + HEIGHT * 0.5f;
            Math::Matrix model =
                Math::Matrix::CreateTranslation({ cx, cy }) * Math::Matrix::CreateScale({ w, HEIGHT });
            tex->Draw(shader, model);
            extLeft += w;
        }
    }

    if (m_thirdThirdTrain)
    {
        const float c4Left = GetCar4LocalLeft();
        float cx = trainLeft + c4Left + m_car4Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car4Width, HEIGHT });
        m_thirdThirdTrain->Draw(shader, model);
    }

    if (m_fourthTrain)
    {
        float cx = trainLeft + GetCar4LocalLeft() + m_car4Width + m_car5Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car5Width, HEIGHT });
        m_fourthTrain->Draw(shader, model);
    }

    if (m_fifthTrain)
    {
        float cx = trainLeft + GetCar4LocalLeft() + m_car4Width + m_car5Width + m_car6Width * 0.5f;
        float cy = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model =
            Math::Matrix::CreateTranslation({ cx, cy }) *
            Math::Matrix::CreateScale({ m_car6Width, HEIGHT });
        m_fifthTrain->Draw(shader, model);
    }

    // Draw Hiding Spot Sprites
    for (const auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            shader.setBool("flipX", false);
            Math::Vec2 worldPos = { trainLeft + spot.localCenter.x, MIN_Y + spot.localCenter.y };
            Math::Matrix spotModel = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(spot.size);
            spot.sprite->Draw(shader, spotModel);
        }
    }

    // Draw PulseBox sprite
    if (m_car2PurpleHbValid && m_pulseBoxSprite)
    {
        shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
        shader.setBool("flipX", false);
        Math::Vec2 worldPos = { trainLeft + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };
        Math::Matrix model = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(m_car2PurpleHb.size);
        m_pulseBoxSprite->Draw(shader, model);
    }

    // Draw Siren (LED) sprite
    if (m_car3SirenHbValid && m_sirenSprite)
    {
        shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
        shader.setBool("flipX", false);
        Math::Vec2 worldPos = { trainLeft + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };
        Math::Matrix model = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(m_car3SirenHb.size);
        m_sirenSprite->Draw(shader, model);
    }

    // -------------------------------------------------------------------------
    // [개발 팀 안내 / 가이드라인] - 중첩 오브젝트의 글로우 아웃라인 렌더링 규칙
    // -------------------------------------------------------------------------
    // 물탱크 칸(Car 5)의 파이프(WaterOpener.png)와 밸브 휠(Valve.png)처럼
    // 화면상에서 물리적으로 중합되거나 겹치는 상하 관계를 가진 오브젝트들의 글로우 아웃라인은
    // 셰이더와 깊이 판정의 영향 없이 올바르게 정렬하기 위해 "인터리브드(Interleaved) 순서"로 그려야 합니다.
    //
    // 1. 하위 오브젝트(예: 파이프) 텍스처 그리기
    // 2. 하위 오브젝트의 블루 글로우 아웃라인 그리기
    // 3. 상위 오브젝트(예: 밸브 휠) 텍스처 그리기 -> 파이프의 블루 글로우 선을 깔끔하게 마스킹(덮음)
    // 4. 상위 오브젝트의 레드 글로우 아웃라인 그리기 -> 최종적으로 밸브 위에 안착
    //
    // 향후 맵 오브젝트 중 서로 겹치면서 각자 독립된 색상의 아웃라인을 가져야 하는 스프라이트가 추가된다면,
    // 전체 아웃라인 패스(DrawSpriteOutlines)에 일괄 배치하는 대신 아래처럼 Draw 함수 내부에
    // 셰이더 스위칭(outlineShader -> textureShader) 코드를 삽입하여 드로우 순서를 제어해야 합니다.
    // -------------------------------------------------------------------------
    if (m_waterOpenerSprite && m_waterOpenerSprite->GetWidth() > 0)
    {
        const Math::Vec2 openerWorld = { trainLeft + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
        Math::Matrix model = Math::Matrix::CreateTranslation(openerWorld) * Math::Matrix::CreateScale({ 330.f, 165.f });
        m_waterOpenerSprite->Draw(shader, model);

        if (outlineShader && projection)
        {
            float distSq = getAABBProximitySq(playerPos, openerWorld, { 330.f, 165.f });
            if (distSq <= proxDistSq)
            {
                outlineShader->use();
                outlineShader->setMat4("projection", *projection);
                outlineShader->setVec2("texelSize", 1.0f / m_waterOpenerSprite->GetWidth(), 1.0f / m_waterOpenerSprite->GetHeight());
                outlineShader->setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f); // Blue glow outline
                m_waterOpenerSprite->Draw(*outlineShader, model);
                shader.use();
            }
        }
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

        if (outlineShader && projection)
        {
            float distSq = getAABBProximitySq(playerPos, valveWorld, m_valveVisualSize);
            if (distSq <= proxDistSq)
            {
                outlineShader->use();
                outlineShader->setMat4("projection", *projection);
                outlineShader->setVec2("texelSize", 1.0f / m_valveSprite->GetWidth(), 1.0f / m_valveSprite->GetHeight());
                outlineShader->setVec4("outlineColor", 1.0f, 0.2f, 0.2f, 1.0f); // Red glow outline
                m_valveSprite->Draw(*outlineShader, model);
                shader.use();
            }
        }
    }

    // ── Robots (none currently, kept for future use) ─────────────────────
    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
            robot.Draw(shader);
    }
}


// ---------------------------------------------------------------------------
// [DrawDrones]
// - 기능: 외부 뷰 상태인 경우 모든 드론(전투, 사이렌, 자동차 운반 등)의 이미지를 그립니다.
// - 매개변수:
//   - shader: 스프라이트 렌더링에 사용할 셰이더 레퍼런스
// - 가이드라인: 내부 뷰나 터널 내부인 경우 연출 스킵 및 드론 노출 상태 분기를 판정합니다.
// ---------------------------------------------------------------------------
void Train::DrawDrones(Shader& shader) const
{
    const bool insideView = m_car3InsideViewActive || m_car3InsideTransitionActive
        || m_car3TunnelInsideViewActive || m_car3TunnelInsideTransitionActive;
    if (!insideView)
    {
        // 외부 뷰: 모든 드론 그리기
        if (m_droneManager)
            m_droneManager->Draw(shader);
        if (m_carTransportDroneManager)
            m_carTransportDroneManager->Draw(shader);
    }
    // 사이렌 드론: 외부 뷰에서는 사이렌 드론, 내부 뷰에서는 inside 드론으로 재사용
    // → 항상 그리기 (터널 인사이드 제외)
    if (!m_car3TunnelInsideViewActive && !m_car3TunnelInsideTransitionActive)
    {
        if (m_sirenDroneManager)
            m_sirenDroneManager->Draw(shader);
    }
}

// ---------------------------------------------------------------------------
// [DrawRadars]
// - 기능: 드론들의 위험 구역 및 인식 거리(레이더 원)를 월드 상에 그립니다.
// - 매개변수:
//   - colorShader: 선 그리기에 사용할 단색 셰이더 레퍼런스
//   - debugRenderer: 렌더링을 실제 수행할 디버그 드로어 객체
// - 가이드라인: 외부 위협 연출 차단(`ShouldHideTrainExteriorHazards`) 상태 시 노출하지 않습니다.
// ---------------------------------------------------------------------------
void Train::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_car3TunnelInsideViewActive || m_car3TunnelInsideTransitionActive)
        return;

    if (!m_car3InsideViewActive && !m_car3InsideTransitionActive && !ShouldHideTrainExteriorHazards())
    {
        if (m_droneManager)
            m_droneManager->DrawRadars(colorShader, debugRenderer);
        if (m_carTransportDroneManager)
            m_carTransportDroneManager->DrawRadars(colorShader, debugRenderer);
    }

    if (m_sirenDroneManager)
        m_sirenDroneManager->DrawRadars(colorShader, debugRenderer);
}

// ---------------------------------------------------------------------------
// [DrawGauges]
// - 기능: 드론들과 로봇들의 HP 바, 또는 경고 게이지를 각 캐릭터 상단에 렌더링합니다.
// - 매개변수:
//   - colorShader: 게이지 사각형을 그릴 셰이더 레퍼런스
//   - debugRenderer: 렌더링을 위임할 디버그 드로어
// - 가이드라인: 각 유닛의 활성화/사망 상태를 우선 판정하여 예외 처리를 수행하십시오.
// ---------------------------------------------------------------------------
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
// [DrawDebug]
// - 기능: 디버그 상태일 때 히트박스(물리적 충돌 영역), 히딩 스팟, 레일 발판 등을 유채색 박스로 시각화합니다.
// - 매개변수:
//   - colorShader: 박스 그리기에 사용할 셰이더 레퍼런스
//   - debugRenderer: 사각형 렌더링용 디버그 렌더러 객체
// - 가이드라인: 히트박스의 로컬 좌표계와 월드 좌표계 변환이 정확한지 검사하는 용도이므로, 크기 보정이 올바른지 확인해야 합니다.
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
    if (m_car3TunnelInsideViewActive)
    {
        const float     railTop = GetRailWalkSurfaceWorldY();
        constexpr float kSlabH  = 36.f;
        const float     y       = railTop - kSlabH * 0.5f;

        // Left rail: [0, 835]
        const Math::Vec2 leftC = { m_tunnelInsideWorldLeft + 835.f * 0.5f, y };
        const Math::Vec2 leftS = { 835.f, kSlabH };
        debugRenderer.DrawBox(colorShader, leftC, leftS, 1.0f, 0.55f, 0.2f);

        // Right rail: [2056, 2640]
        const Math::Vec2 rightC = { m_tunnelInsideWorldLeft + 2056.f + 584.f * 0.5f, y };
        const Math::Vec2 rightS = { 584.f, kSlabH };
        debugRenderer.DrawBox(colorShader, rightC, rightS, 1.0f, 0.55f, 0.2f);
    }
    else
    {
        for (const auto& hb : m_staticWorldHitboxes)
        {
            const Math::Vec2 worldPos = { MIN_X + hb.localCenter.x, MIN_Y + hb.localCenter.y };
            debugRenderer.DrawBox(colorShader, worldPos, hb.size, 1.0f, 0.55f, 0.2f);
        }
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

    if (m_car3ExtensionEnterHbValid)
    {
        const Math::Vec2 worldPos = { trainLeft + m_car3ExtensionEnterHb.localCenter.x, MIN_Y + m_car3ExtensionEnterHb.localCenter.y };
        debugRenderer.DrawBox(colorShader, worldPos, m_car3ExtensionEnterHb.size, 1.0f, 0.2f, 1.0f);
    }
    if (m_car3TunnelEnterHbValid)
    {
        const Math::Vec2 worldPos = { trainLeft + m_car3TunnelEnterHb.localCenter.x, MIN_Y + m_car3TunnelEnterHb.localCenter.y };
        debugRenderer.DrawBox(colorShader, worldPos, m_car3TunnelEnterHb.size, 0.9f, 0.55f, 0.15f);
    }

    if (m_car3InsideLadderHbValid)
    {
        const Math::Vec2 ladderPos = { trainLeft + m_car3InsideLadderHb.localCenter.x, MIN_Y + m_car3InsideLadderHb.localCenter.y };
        const Math::Vec2 ladder2Pos = { trainLeft + m_car3InsideLadder2Hb.localCenter.x, MIN_Y + m_car3InsideLadder2Hb.localCenter.y };
        debugRenderer.DrawBox(colorShader, ladderPos, m_car3InsideLadderHb.size, 0.2f, 1.0f, 0.4f);
        debugRenderer.DrawBox(colorShader, ladder2Pos, m_car3InsideLadder2Hb.size, 0.2f, 0.9f, 0.2f);
        const Math::Vec2 floorPos = { trainLeft + m_car3InsideFloorHb.localCenter.x, MIN_Y + m_car3InsideFloorHb.localCenter.y };
        debugRenderer.DrawBox(colorShader, floorPos, m_car3InsideFloorHb.size, 0.3f, 0.8f, 1.0f);
        const Math::Vec2 floor2Pos = { trainLeft + m_car3InsideFloor2Hb.localCenter.x, MIN_Y + m_car3InsideFloor2Hb.localCenter.y };
        debugRenderer.DrawBox(colorShader, floor2Pos, m_car3InsideFloor2Hb.size, 0.3f, 0.7f, 1.0f);
        const Math::Vec2 floor3Pos = { trainLeft + m_car3InsideFloor3Hb.localCenter.x, MIN_Y + m_car3InsideFloor3Hb.localCenter.y };
        debugRenderer.DrawBox(colorShader, floor3Pos, m_car3InsideFloor3Hb.size, 0.3f, 0.6f, 1.0f);
        const Math::Vec2 ceilPos = { trainLeft + m_car3InsideCeilingHb.localCenter.x, MIN_Y + m_car3InsideCeilingHb.localCenter.y };
        debugRenderer.DrawBox(colorShader, ceilPos, m_car3InsideCeilingHb.size, 1.0f, 0.4f, 0.4f);
        const Math::Vec2 roofPos = { trainLeft + m_car3InsideRoofHb.localCenter.x, MIN_Y + m_car3InsideRoofHb.localCenter.y };
        debugRenderer.DrawBox(colorShader, roofPos, m_car3InsideRoofHb.size, 1.0f, 0.85f, 0.2f);
        const float ext1Local = m_car1Width + m_car2Width + m_car3Width;
        const float boundLeft  = trainLeft + ext1Local + kCar3InsideBoundLeftPx;
        const float boundRightFloor = trainLeft + ext1Local + kCar3InsideBoundRightPx;
        const float boundRightRoof  = trainLeft + ext1Local + m_car3ExtensionWidths[0] + kCar3InsideBoundRightPx;
        const float boundRightFloor3 = trainLeft + ext1Local + m_car3ExtensionWidths[0] + m_car3ExtensionWidths[1]
            + std::min(kCar3InsideBoundRightPx, std::max(400.f, m_car3ExtensionWidths[2] - 84.f));
        debugRenderer.DrawBox(colorShader, { boundLeft, MIN_Y + HEIGHT * 0.5f }, { 6.f, HEIGHT }, 1.f, 0.f, 1.f);
        debugRenderer.DrawBox(colorShader, { boundRightFloor, MIN_Y + HEIGHT * 0.5f }, { 6.f, HEIGHT * 0.55f }, 1.f, 0.f, 1.f);
        debugRenderer.DrawBox(colorShader, { boundRightFloor3, MIN_Y + HEIGHT * 0.5f }, { 6.f, HEIGHT * 0.55f }, 0.2f, 1.f, 0.8f);
        debugRenderer.DrawBox(colorShader, { boundRightRoof, MIN_Y + HEIGHT * 0.5f }, { 6.f, HEIGHT * 0.45f }, 0.2f, 1.f, 0.4f);
    }

    if (m_car3TunnelInsideViewActive)
    {
        for (const auto& prop : m_tunnelInsideProps)
        {
            const Math::Vec2 worldPos = { m_tunnelInsideWorldLeft + prop.localCenter.x,
                                          MIN_Y + prop.localCenter.y };
            const float r = prop.pushable ? 1.0f : 0.55f;
            const float g = prop.pushable ? 0.85f : 0.35f;
            const float b = prop.pushable ? 0.2f : 0.95f;
            debugRenderer.DrawBox(colorShader, worldPos, prop.size, r, g, b);
        }

        // Always draw boarding floor debug box in TunnelInside view
        {
            Math::Vec2 boardC{};
            Math::Vec2 boardS{};
            GetTunnelInsideBoardingFloor(boardC, boardS);
            debugRenderer.DrawBox(colorShader, boardC, boardS, 1.0f, 0.92f, 0.15f);
        }

        if (m_tunnelInsideDepartStarted)
        {
            Math::Vec2 walkC{};
            Math::Vec2 walkS{};
            GetTunnelInsideDepartWalkFloor(walkC, walkS);
            debugRenderer.DrawBox(colorShader, walkC, walkS, 0.25f, 0.85f, 1.0f);
        }
    }

    // Map boundary markers (white)
    const float bndH = HEIGHT;
    debugRenderer.DrawBox(colorShader,
        { MIN_X,         MIN_Y + bndH * 0.5f }, { 10.0f, bndH }, { 1.0f, 1.0f });
    debugRenderer.DrawBox(colorShader,
        { MIN_X + m_totalTrainWidth, MIN_Y + bndH * 0.5f }, { 10.0f, bndH }, { 1.0f, 1.0f });
}

void Train::DrawSpriteOutlines(Shader& outlineShader, Math::Vec2 playerPos, float proximityDist) const
{
    const float proxDistSq = proximityDist * proximityDist;
    const float trainLeft = MIN_X + m_trainOffset;

    auto getAABBProximitySq = [](Math::Vec2 pPos, Math::Vec2 boxPos, Math::Vec2 boxSize) {
        float dx = std::max(0.0f, std::abs(pPos.x - boxPos.x) - boxSize.x * 0.5f);
        float dy = std::max(0.0f, std::abs(pPos.y - boxPos.y) - boxSize.y * 0.5f);
        return dx * dx + dy * dy;
    };

    if (m_car3TunnelInsideViewActive)
    {
        // Draw ONLY Tunnel Pulse Injector outline when inside tunnel
        if (m_tunnelPulseInjectorSprite)
        {
            for (const auto& prop : m_tunnelInsideProps)
            {
                if (!prop.injectable) continue;
                Math::Vec2 worldPos = { m_tunnelInsideWorldLeft + prop.localCenter.x, MIN_Y + prop.localCenter.y };
                float distSq = getAABBProximitySq(playerPos, worldPos, prop.size);
                if (distSq <= proxDistSq)
                {
                    int w = m_tunnelPulseInjectorSprite->GetWidth();
                    int h = m_tunnelPulseInjectorSprite->GetHeight();
                    if (w > 0 && h > 0)
                    {
                        outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
                        outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f); // Blue glow outline

                        Math::Matrix model = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(prop.size);
                        m_tunnelPulseInjectorSprite->Draw(outlineShader, model);
                    }
                }
            }
        }
        return; // Early return to prevent drawing main train outlines!
    }

    // Draw Hiding Spots outlines
    for (const auto& spot : m_hidingSpots)
    {
        if (!spot.sprite) continue;
        Math::Vec2 worldPos = { trainLeft + spot.localCenter.x, MIN_Y + spot.localCenter.y };
        float distSq = getAABBProximitySq(playerPos, worldPos, spot.size);
        if (distSq <= proxDistSq)
        {
            int w = spot.sprite->GetWidth();
            int h = spot.sprite->GetHeight();
            if (w <= 0 || h <= 0) continue;

            outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
            outlineShader.setVec4("outlineColor", 0.15f, 1.0f, 0.35f, 1.0f); // Green glow outline

            Math::Matrix model = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(spot.size);
            spot.sprite->Draw(outlineShader, model);
        }
    }

    // Draw PulseBox outline
    if (m_car2PurpleHbValid && m_pulseBoxSprite)
    {
        Math::Vec2 worldPos = { trainLeft + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };
        float distSq = getAABBProximitySq(playerPos, worldPos, m_car2PurpleHb.size);
        if (distSq <= proxDistSq)
        {
            int w = m_pulseBoxSprite->GetWidth();
            int h = m_pulseBoxSprite->GetHeight();
            if (w > 0 && h > 0)
            {
                outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
                outlineShader.setVec4("outlineColor", 0.8f, 0.2f, 1.0f, 1.0f); // Purple glow outline

                Math::Matrix model = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(m_car2PurpleHb.size);
                m_pulseBoxSprite->Draw(outlineShader, model);
            }
        }
    }

    // Draw Siren LED outline
    if (m_car3SirenHbValid && m_sirenSprite)
    {
        Math::Vec2 worldPos = { trainLeft + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };
        float distSq = getAABBProximitySq(playerPos, worldPos, m_car3SirenHb.size);
        if (distSq <= proxDistSq)
        {
            int w = m_sirenSprite->GetWidth();
            int h = m_sirenSprite->GetHeight();
            if (w > 0 && h > 0)
            {
                outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
                outlineShader.setVec4("outlineColor", 0.2f, 0.6f, 1.0f, 1.0f); // Blue glow outline

                Math::Matrix model = Math::Matrix::CreateTranslation(worldPos) * Math::Matrix::CreateScale(m_car3SirenHb.size);
                m_sirenSprite->Draw(outlineShader, model);
            }
        }
    }
}

void Train::DrawSecondTrain3Foreground(Shader& shader, Math::Vec2 playerPos) const
{
    if (!m_car3InsideViewActive || m_car3InsideTransitionActive || m_car3InsideOnRoof)
        return;

    if (m_car3TunnelInsideTransitionActive)
        return;

    const float trainLeft = MIN_X + m_trainOffset;
    const float lx = playerPos.x - trainLeft;
    
    // Green boundary: 12261.f (inside2Right boundary = ext1Local + extension[0] + kCar3InsideBoundRightPx)
    const float greenBoundary = m_car1Width + m_car2Width + m_car3Width 
                              + m_car3ExtensionWidths[0] + kCar3InsideBoundRightPx;
    
    if (lx >= greenBoundary)
    {
        // 1) Draw the rightmost portion of SecondInside_2 (index 1) which player walks behind
        Background* texInside2 = m_car3InsideTrainB ? m_car3InsideTrainB.get() : m_car3ExtensionTrains[1].get();
        if (texInside2 && texInside2->GetWidth() > 0)
        {
            const float extLeftInside2 = trainLeft + m_car1Width + m_car2Width + m_car3Width 
                                       + m_car3ExtensionWidths[0];
            const float wInside2 = m_car3ExtensionWidths[1];
            const float sliceStart = kCar3InsideBoundRightPx;
            const float sliceW = wInside2 - sliceStart;
            
            const float cx = extLeftInside2 + sliceStart + sliceW * 0.5f;
            const float cy = MIN_Y + HEIGHT * 0.5f;
            Math::Matrix model =
                Math::Matrix::CreateTranslation({ cx, cy }) * Math::Matrix::CreateScale({ sliceW, HEIGHT });
            
            shader.setBool("flipX", false);
            shader.setFloat("alpha", 1.0f);
            
            texInside2->Draw(shader, model, sliceStart / wInside2, 0.0f, sliceW / wInside2, 1.0f);
        }

        // 2) Draw the entire SecondTrain_3 (index 2) as overlay
        Background* tex3 = m_car3ExtensionTrains[2].get();
        if (tex3 && tex3->GetWidth() > 0)
        {
            const float extLeft3 = trainLeft + m_car1Width + m_car2Width + m_car3Width 
                                 + m_car3ExtensionWidths[0] + m_car3ExtensionWidths[1];
            const float w3 = m_car3ExtensionWidths[2];
            const float cx = extLeft3 + w3 * 0.5f;
            const float cy = MIN_Y + HEIGHT * 0.5f;
            Math::Matrix model =
                Math::Matrix::CreateTranslation({ cx, cy }) * Math::Matrix::CreateScale({ w3, HEIGHT });
            
            shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
            shader.setBool("flipX", false);
            shader.setFloat("alpha", 1.0f);
            
            tex3->Draw(shader, model);
        }
    }
}
