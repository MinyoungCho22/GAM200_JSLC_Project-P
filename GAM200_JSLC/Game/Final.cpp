// Final.cpp

#include "Final.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <algorithm>
#include <cmath>

void Final::Initialize()
{
    m_final1 = std::make_unique<Background>();
    m_final1->Initialize("Asset/Train/Final_1.png");

    m_final2 = std::make_unique<Background>();
    m_final2->Initialize("Asset/Train/Final_2.png");

    m_cityLast = std::make_unique<Background>();
    m_cityLast->Initialize("Asset/Train/City_Last.png");

    m_cityMiddle = std::make_unique<Background>();
    m_cityMiddle->Initialize("Asset/Train/City_Middle.png");

    m_cityFront = std::make_unique<Background>();
    m_cityFront->Initialize("Asset/Train/City_Front.png");

    float w1 = m_final1->GetWidth() > 0 ? static_cast<float>(m_final1->GetWidth()) : 3960.0f;
    float w2 = m_final2->GetWidth() > 0 ? static_cast<float>(m_final2->GetWidth()) : 3960.0f;
    m_mapWidth = w1 + w2;

    InitSkyVAO();
}

void Final::InitSkyVAO()
{
    float vertices[] = {
        -0.5f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f
    };
    GL::GenVertexArrays(1, &m_skyVAO);
    GL::GenBuffers(1, &m_skyVBO);
    GL::BindVertexArray(m_skyVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_skyVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::BindVertexArray(0);
}

void Final::DrawFilledQuad(Shader& colorShader, Math::Vec2 center, Math::Vec2 size,
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

void Final::DrawParallaxBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    viewHalfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float skyAnchorY = MIN_Y + HEIGHT * 0.5f;
    const float skyLift    = cameraPos.y - skyAnchorY;
    const auto  relY       = [&](float t) { return MIN_Y + HEIGHT * t + skyLift; };
    const float spanW      = viewHalfW * 2.0f + 1200.0f;
    const float skyPx      = cameraPos.x;

    // Darkened sunset gradient (colors scaled by 0.25f)
    const float darkness = 0.25f;
    DrawFilledQuad(colorShader, { skyPx, relY(0.90f) }, { spanW, HEIGHT * 0.22f }, 0.13f * darkness, 0.05f * darkness, 0.19f * darkness, 1.0f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.75f) }, { spanW, HEIGHT * 0.22f }, 0.22f * darkness, 0.08f * darkness, 0.20f * darkness, 0.95f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.60f) }, { spanW, HEIGHT * 0.20f }, 0.38f * darkness, 0.11f * darkness, 0.18f * darkness, 0.90f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.47f) }, { spanW, HEIGHT * 0.18f }, 0.58f * darkness, 0.17f * darkness, 0.14f * darkness, 0.88f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.36f) }, { spanW, HEIGHT * 0.16f }, 0.80f * darkness, 0.28f * darkness, 0.11f * darkness, 0.85f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.25f) }, { spanW, HEIGHT * 0.18f }, 0.53f * darkness, 0.18f * darkness, 0.10f * darkness, 0.70f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.11f) }, { spanW, HEIGHT * 0.22f }, 0.10f * darkness, 0.07f * darkness, 0.08f * darkness, 1.0f);

    // Repeated clouds (dimmed by 0.25f)
    const float farPx  = MIN_X + (cameraPos.x - MIN_X) * 0.05f;
    const float cloudBase = farPx;
    const float cloudStep = 620.0f;
    const float visibleLeft  = cameraPos.x - viewHalfW - 700.0f;
    const float visibleRight = cameraPos.x + viewHalfW + 700.0f;
    const int cloudMinI = static_cast<int>(std::floor((visibleLeft - cloudBase) / cloudStep));
    const int cloudMaxI = static_cast<int>(std::ceil((visibleRight - cloudBase) / cloudStep));
    for (int i = cloudMinI; i <= cloudMaxI; ++i)
    {
        const float x = cloudBase + i * 620.0f;
        const float y1 = relY(0.78f - 0.02f * static_cast<float>((i + 30) % 4));
        const float y2 = relY(0.66f - 0.02f * static_cast<float>((i + 11) % 5));
        const float y3 = relY(0.56f - 0.015f * static_cast<float>((i + 7) % 6));

        DrawFilledQuad(colorShader, { x,          y1 }, { 520.0f, 52.0f }, 0.40f * darkness, 0.17f * darkness, 0.27f * darkness, 0.26f);
        DrawFilledQuad(colorShader, { x + 120.0f, y1 - 24.0f }, { 360.0f, 38.0f }, 0.33f * darkness, 0.13f * darkness, 0.24f * darkness, 0.20f);

        DrawFilledQuad(colorShader, { x - 80.0f,  y2 }, { 430.0f, 42.0f }, 0.52f * darkness, 0.21f * darkness, 0.20f * darkness, 0.18f);
        DrawFilledQuad(colorShader, { x + 50.0f,  y2 - 20.0f }, { 300.0f, 30.0f }, 0.45f * darkness, 0.17f * darkness, 0.18f * darkness, 0.14f);

        DrawFilledQuad(colorShader, { x + 30.0f,  y3 }, { 340.0f, 28.0f }, 0.68f * darkness, 0.26f * darkness, 0.16f * darkness, 0.10f);
    }
}

void Final::DrawParallaxLayer(Shader& shader, Background& bg, Math::Vec2 cameraPos, float viewHalfW, float scrollSpeedFactor)
{
    float width = bg.GetWidth();
    if (width <= 0.0f) width = 1920.0f;

    // Scroll position relative to map origin MIN_X
    float startX = MIN_X + (cameraPos.x - MIN_X) * scrollSpeedFactor;

    // Determine visible range
    float visibleLeft  = cameraPos.x - viewHalfW - width;
    float visibleRight = cameraPos.x + viewHalfW + width;

    // Tile indices to cover [visibleLeft, visibleRight]
    int minI = static_cast<int>(std::floor((visibleLeft - startX) / width));
    int maxI = static_cast<int>(std::ceil((visibleRight - startX) / width));

    for (int i = minI; i <= maxI; ++i)
    {
        float x = startX + i * width + width * 0.5f;
        float y = MIN_Y + HEIGHT * 0.5f;
        Math::Matrix model = Math::Matrix::CreateTranslation({ x, y }) * Math::Matrix::CreateScale({ width, HEIGHT });
        bg.Draw(shader, model);
    }
}

void Final::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    (void)dt;
    Math::Vec2 pos = player.GetPosition();
    Math::Vec2 halfSize = playerHitboxSize * 0.5f;
    float minX = MIN_X + halfSize.x;
    float maxX = MIN_X + m_mapWidth - halfSize.x;
    if (pos.x < minX)
    {
        pos.x = minX;
        player.SetPosition(pos);
    }
    else if (pos.x > maxX)
    {
        pos.x = maxX;
        player.SetPosition(pos);
    }
}

void Final::Draw(Shader& shader, Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW)
{
    // 1. Sunset parallax sky
    DrawParallaxBackground(colorShader, cameraPos, viewHalfW);

    // 2. Parallax city layers
    if (m_cityLast)
        DrawParallaxLayer(shader, *m_cityLast, cameraPos, viewHalfW, 0.05f);
    if (m_cityMiddle)
        DrawParallaxLayer(shader, *m_cityMiddle, cameraPos, viewHalfW, 0.15f);
    if (m_cityFront)
        DrawParallaxLayer(shader, *m_cityFront, cameraPos, viewHalfW, 0.30f);

    // 3. Main map backgrounds
    float w1 = m_final1->GetWidth() > 0 ? static_cast<float>(m_final1->GetWidth()) : 3960.0f;
    float w2 = m_final2->GetWidth() > 0 ? static_cast<float>(m_final2->GetWidth()) : 3960.0f;

    if (m_final1)
    {
        Math::Matrix model = Math::Matrix::CreateTranslation({ MIN_X + w1 * 0.5f, MIN_Y + HEIGHT * 0.5f })
                           * Math::Matrix::CreateScale({ w1, HEIGHT });
        m_final1->Draw(shader, model);
    }
    if (m_final2)
    {
        Math::Matrix model = Math::Matrix::CreateTranslation({ MIN_X + w1 + w2 * 0.5f, MIN_Y + HEIGHT * 0.5f })
                           * Math::Matrix::CreateScale({ w2, HEIGHT });
        m_final2->Draw(shader, model);
    }
}

void Final::Shutdown()
{
    if (m_skyVAO != 0)
    {
        GL::DeleteVertexArrays(1, &m_skyVAO);
        m_skyVAO = 0;
    }
    if (m_skyVBO != 0)
    {
        GL::DeleteBuffers(1, &m_skyVBO);
        m_skyVBO = 0;
    }
    if (m_final1) m_final1->Shutdown();
    if (m_final2) m_final2->Shutdown();
    if (m_cityLast) m_cityLast->Shutdown();
    if (m_cityMiddle) m_cityMiddle->Shutdown();
    if (m_cityFront) m_cityFront->Shutdown();
}
