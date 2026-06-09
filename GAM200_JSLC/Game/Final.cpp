// Final.cpp

#include "Final.hpp"
#include "Player.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/DebugRenderer.hpp"
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

    m_hitboxes.clear();

    // Yellow / Tall blocks (Yellow: 348x662)
    // Coords from image: (537, 686), (2325, 686), (3507, 825)
    auto makeYellowHitbox = [&](float px, float py) {
        float pw = 348.0f;
        float ph = 662.0f;
        float cx = MIN_X + w1 + px + pw * 0.5f;
        float cy = MIN_Y + (1080.0f - py - ph * 0.5f);
        return Hitbox{ {cx, cy}, {pw, ph}, true };
    };

    m_hitboxes.push_back(makeYellowHitbox(537.0f, 24.0f));
    m_hitboxes.push_back(makeYellowHitbox(2325.0f, 24.0f));
    m_hitboxes.push_back(makeYellowHitbox(3507.0f, 163.0f));

    // Orange / Low slabs (Orange: 252x203)
    // Coords from image: (453, 944), (1436, 944), (2421, 944)
    auto makeOrangeHitbox = [&](float px, float py) {
        float pw = 252.0f;
        float ph = 203.0f;
        float cx = MIN_X + w1 + px + pw * 0.5f;
        float cy = MIN_Y + (1080.0f - py - ph * 0.5f);
        return Hitbox{ {cx, cy}, {pw, ph}, false };
    };

    m_hitboxes.push_back(makeOrangeHitbox(453.0f, 741.0f));
    m_hitboxes.push_back(makeOrangeHitbox(1436.0f, 741.0f));
    m_hitboxes.push_back(makeOrangeHitbox(2421.0f, 741.0f));

    m_pulseLine = std::make_unique<Background>();
    m_pulseLine->Initialize("Asset/pulse/pulse_line_h.png");

    m_sweeps.clear();
    m_sweepSpawnTimer = 0.0f;
    m_spawnDirectionAlternate = false;

    InitSkyVAO();

    m_pulseVentSprite = std::make_unique<Background>();
    m_pulseVentSprite->Initialize("Asset/Train/Pulse_Vent.png");

    m_pulseSources.clear();
    m_pulseSources.resize(3);
    for (int i = 0; i < 3; ++i)
    {
        const auto& hb = m_hitboxes[3 + i]; // Orange slab hitboxes are indices 3, 4, 5
        m_pulseSources[i].Initialize(hb.pos, hb.size, 100.0f);
        m_pulseSources[i].SetPulseAmount(0.0f); // initially empty/inactive
    }

    m_activeVentIndex = rand() % 3;
    m_pulseSources[m_activeVentIndex].RefillStock();
    m_ventTimer = 0.0f;

    // Initialize Boss (identical to player assets)
    m_boss.Init({ Final::MIN_X + 3960.0f + 1436.0f, 0.0f });
    m_boss.SetSizeScale(0.6f);
    float bossY = (Final::MIN_Y + 258.0f) + m_boss.GetHitboxSize().y * 0.5f;
    m_boss.SetPosition({ Final::MIN_X + 3960.0f + 1436.0f, bossY });
    m_boss.SetCurrentGroundLevel(Final::MIN_Y + 258.0f);
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

    // Deep twilight/night blue gradient (rich and dark twilight sunset colors)
    DrawFilledQuad(colorShader, { skyPx, relY(0.90f) }, { spanW, HEIGHT * 0.22f }, 0.003f, 0.005f, 0.03f, 1.0f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.75f) }, { spanW, HEIGHT * 0.22f }, 0.006f, 0.01f, 0.06f, 0.98f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.60f) }, { spanW, HEIGHT * 0.20f }, 0.012f, 0.018f, 0.11f, 0.95f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.47f) }, { spanW, HEIGHT * 0.18f }, 0.02f, 0.028f, 0.18f, 0.92f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.36f) }, { spanW, HEIGHT * 0.16f }, 0.035f, 0.04f, 0.24f, 0.88f);
    DrawFilledQuad(colorShader, { skyPx, relY(0.25f) }, { spanW, HEIGHT * 0.18f }, 0.05f, 0.035f, 0.18f, 0.82f); // twilight purple-blue glow
    DrawFilledQuad(colorShader, { skyPx, relY(0.11f) }, { spanW, HEIGHT * 0.22f }, 0.08f, 0.025f, 0.12f, 1.0f); // horizon deep dark sunset crimson-purple

    // Repeated clouds (dark navy/blackish clouds to match twilight style)
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

        DrawFilledQuad(colorShader, { x,          y1 }, { 520.0f, 52.0f }, 0.0f, 0.03f, 0.12f, 0.35f);
        DrawFilledQuad(colorShader, { x + 120.0f, y1 - 24.0f }, { 360.0f, 38.0f }, 0.0f, 0.02f, 0.10f, 0.25f);

        DrawFilledQuad(colorShader, { x - 80.0f,  y2 }, { 430.0f, 42.0f }, 0.0f, 0.02f, 0.10f, 0.30f);
        DrawFilledQuad(colorShader, { x + 50.0f,  y2 - 20.0f }, { 300.0f, 30.0f }, 0.0f, 0.01f, 0.08f, 0.20f);

        DrawFilledQuad(colorShader, { x + 30.0f,  y3 }, { 340.0f, 28.0f }, 0.0f, 0.01f, 0.08f, 0.20f);
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

    // Floor Pulse Sweep update
    m_sweepSpawnTimer += static_cast<float>(dt);
    if (m_sweepSpawnTimer >= 3.5f)
    {
        FloorSweep sweep;
        sweep.width = 240.0f;
        sweep.damagedPlayer = false;
        if (m_spawnDirectionAlternate)
        {
            // Move from right to left
            sweep.x = MIN_X + 7920.0f;
            sweep.speed = -450.0f;
        }
        else
        {
            // Move from left to right
            sweep.x = MIN_X + 3960.0f;
            sweep.speed = 450.0f;
        }
        m_sweeps.push_back(sweep);
        m_spawnDirectionAlternate = !m_spawnDirectionAlternate;
        m_sweepSpawnTimer = 0.0f;
    }

    float playerX = player.GetPosition().x;
    float halfW = playerHitboxSize.x * 0.5f;

    for (auto it = m_sweeps.begin(); it != m_sweeps.end(); )
    {
        it->x += it->speed * static_cast<float>(dt);

        // Check collision with the player on the ground
        if (player.IsOnGround() && !it->damagedPlayer && !player.IsGodMode())
        {
            float sweepLeft = it->x - it->width * 0.5f;
            float sweepRight = it->x + it->width * 0.5f;
            if (playerX + halfW >= sweepLeft && playerX - halfW <= sweepRight)
            {
                player.TakeDamage(15.0f); // Deal 15.0f pulse damage to player
                it->damagedPlayer = true;
            }
        }

        // Clean up off-screen sweeps
        bool outOfBounds = false;
        if (it->speed > 0.0f && it->x > MIN_X + 7920.0f + 200.0f)
            outOfBounds = true;
        else if (it->speed < 0.0f && it->x < MIN_X + 3960.0f - 200.0f)
            outOfBounds = true;

        if (outOfBounds)
        {
            it = m_sweeps.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Vent state cycling update
    m_ventTimer += static_cast<float>(dt);
    if (m_ventTimer >= VENT_CYCLE_DURATION)
    {
        m_ventTimer = 0.0f;
        if (m_activeVentIndex != -1 && m_activeVentIndex < 3)
            m_pulseSources[m_activeVentIndex].SetPulseAmount(0.0f);

        m_activeVentIndex = rand() % 3;
        m_pulseSources[m_activeVentIndex].RefillStock();
    }
    else if (m_ventTimer >= VENT_ACTIVE_DURATION)
    {
        if (m_activeVentIndex != -1 && m_activeVentIndex < 3)
            m_pulseSources[m_activeVentIndex].SetPulseAmount(0.0f);
        m_activeVentIndex = -1;
    }

    // Update Boss Animation
    m_boss.UpdateNPC(static_cast<float>(dt), AnimationState::Idle);
}

void Final::Draw(Shader& shader, Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW, const Math::Matrix& projection)
{
    // 1. Sunset parallax sky
    colorShader.use();
    colorShader.setMat4("projection", projection);
    DrawParallaxBackground(colorShader, cameraPos, viewHalfW);

    // Restore shader state for parallax layers
    shader.use();
    shader.setMat4("projection", projection);

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

    // Draw Boss
    m_boss.Draw(shader);

    // 4. Floor Pulse Sweep line drawing
    if (m_pulseLine)
    {
        shader.use();
        shader.setBool("flipX", false);
        shader.setFloat("alpha", 1.0f);
        shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);

        float tileW = 32.0f;
        float startX = MIN_X + 3960.0f; // start of Final_2.png
        float endX = MIN_X + m_mapWidth; // end of map
        float y = MIN_Y + 258.0f + 4.0f; // sit on top of 258.0f deck (raised slightly)

        for (float tx = startX + tileW * 0.5f; tx <= endX; tx += tileW)
        {
            bool isActive = false;
            for (const auto& sweep : m_sweeps)
            {
                float sweepLeft = sweep.x - sweep.width * 0.5f;
                float sweepRight = sweep.x + sweep.width * 0.5f;
                if (tx >= sweepLeft && tx <= sweepRight)
                {
                    isActive = true;
                    break;
                }
            }

            if (isActive)
            {
                shader.setVec3("colorTint", 0.0f, 1.0f, 1.0f); // cyan glow
                shader.setFloat("tintStrength", 1.0f);
            }
            else
            {
                shader.setVec3("colorTint", 0.15f, 0.15f, 0.25f); // dark blue/grey
                shader.setFloat("tintStrength", 1.0f);
            }

            Math::Matrix model = Math::Matrix::CreateTranslation({ tx, y })
                               * Math::Matrix::CreateScale({ tileW, tileW });
            m_pulseLine->Draw(shader, model);
        }

        // Reset tint state
        shader.setFloat("tintStrength", 0.0f);
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    }
}

void Final::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& obs : m_hitboxes)
    {
        if (obs.isYellow)
        {
            debugRenderer.DrawBox(colorShader, obs.pos, obs.size, 1.0f, 1.0f, 0.0f);
        }
        else
        {
            debugRenderer.DrawBox(colorShader, obs.pos, obs.size, 1.0f, 0.5f, 0.0f);
        }
    }
}

void Final::DrawPulseVents(Shader& shader, Shader& outlineShader, Math::Vec2 cameraPos, float viewHalfW)
{
    if (m_activeVentIndex == -1 || !m_pulseVentSprite) return;

    // Fast 0.25 second rising / falling animation
    float riseRatio = 0.0f;
    const float riseTime = 0.25f;
    if (m_ventTimer < riseTime)
    {
        riseRatio = m_ventTimer / riseTime;
    }
    else if (m_ventTimer < VENT_ACTIVE_DURATION - riseTime)
    {
        riseRatio = 1.0f;
    }
    else if (m_ventTimer < VENT_ACTIVE_DURATION)
    {
        riseRatio = (VENT_ACTIVE_DURATION - m_ventTimer) / riseTime;
    }
    else
    {
        riseRatio = 0.0f;
    }

    if (riseRatio <= 0.0f) return;

    const auto& hb = m_hitboxes[3 + m_activeVentIndex];
    Math::Vec2 renderPos = hb.pos;
    
    // Pin bottom edge to floor (which is hb.pos.y - 90.0f)
    renderPos.y = (hb.pos.y - 90.0f) + (180.0f * riseRatio) * 0.5f;
    Math::Vec2 spriteSize = { 156.0f, 180.0f * riseRatio };

    Math::Matrix model = Math::Matrix::CreateTranslation(renderPos) * Math::Matrix::CreateScale(spriteSize);

    // 1. Draw with normal texture shader
    shader.use();
    shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
    shader.setBool("flipX", false);
    shader.setFloat("alpha", 1.0f);
    shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    shader.setFloat("tintStrength", 0.0f);
    m_pulseVentSprite->Draw(shader, model);

    // 2. Draw outline overlay with fire glow
    int w = m_pulseVentSprite->GetWidth();
    int h = m_pulseVentSprite->GetHeight();
    if (w > 0 && h > 0)
    {
        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        outlineShader.use();
        outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
        outlineShader.setVec4("outlineColor", 1.0f, 0.35f, 0.0f, 1.0f); // fiery orange outline
        outlineShader.setFloat("outlineWidthTexels", 2.0f);
        outlineShader.setFloat("uTime", static_cast<float>(glfwGetTime()));
        outlineShader.setBool("isFireGlow", true);

        m_pulseVentSprite->Draw(outlineShader, model);

        // Reset isFireGlow uniform
        outlineShader.setBool("isFireGlow", false);
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
    if (m_pulseLine) m_pulseLine->Shutdown();
    if (m_pulseVentSprite)
    {
        m_pulseVentSprite->Shutdown();
        m_pulseVentSprite.reset();
    }
    for (auto& src : m_pulseSources) src.Shutdown();
    m_boss.Shutdown();
}
