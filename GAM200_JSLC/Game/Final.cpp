// Final.cpp

#include "Final.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
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

    // Yellow / Tall blocks (348x662)
    // Layout label: px = left edge, py = bottom edge (image Y-down). Convert to world center.
    auto makeYellowHitbox = [&](float px, float pyBottom) {
        float pw = 348.0f;
        float ph = 662.0f;
        float cx = MIN_X + w1 + px + pw * 0.5f;
        float cy = MIN_Y + (HEIGHT - pyBottom + ph * 0.5f);
        return Hitbox{ {cx, cy}, {pw, ph}, true };
    };

    m_hitboxes.push_back(makeYellowHitbox(537.0f, 686.0f));
    m_hitboxes.push_back(makeYellowHitbox(2325.0f, 686.0f));
    m_hitboxes.push_back(makeYellowHitbox(3507.0f, 825.0f));

    // Orange / Low slabs (252x203) — same layout convention as yellow blocks
    auto makeOrangeHitbox = [&](float px, float pyBottom) {
        float pw = 252.0f;
        float ph = 203.0f;
        float cx = MIN_X + w1 + px + pw * 0.5f;
        float cy = MIN_Y + (HEIGHT - pyBottom + ph * 0.5f);
        return Hitbox{ {cx, cy}, {pw, ph}, false };
    };

    m_hitboxes.push_back(makeOrangeHitbox(453.0f, 944.0f));
    m_hitboxes.push_back(makeOrangeHitbox(1436.0f, 944.0f));
    m_hitboxes.push_back(makeOrangeHitbox(2421.0f, 944.0f));

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

    // Initialize Boss (identical to player assets, same scale so Y aligns with player)
    m_boss.Init({ Final::MIN_X + 3960.0f + 1436.0f, 0.0f });
    m_boss.SetSizeScale(1.0f);
    float bossY = (Final::MIN_Y + 258.0f) + m_boss.GetHitboxSize().y * 0.5f;
    m_boss.SetPosition({ Final::MIN_X + 3960.0f + 1436.0f, bossY });
    m_boss.SetCurrentGroundLevel(Final::MIN_Y + 258.0f);

    // Setup Overload Devices — interaction hitbox on visible purple panel inside pillar
    auto setupOverloadDevice = [&](int deviceIdx, int pillarIdx) {
        const auto& pillar = m_hitboxes[pillarIdx];
        constexpr float kDeviceW = 280.0f;
        constexpr float kDeviceH = 380.0f;
        const float pillarBottom = pillar.pos.y - pillar.size.y * 0.5f;

        m_overloadDevices[deviceIdx].size = { kDeviceW, kDeviceH };
        m_overloadDevices[deviceIdx].pos  = {
            pillar.pos.x,
            pillarBottom + kDeviceH * 0.5f + 24.0f
        };
        m_overloadDevices[deviceIdx].charge = 0.0f;
        m_overloadDevices[deviceIdx].isOverloaded = false;
    };
    setupOverloadDevice(0, 0);
    setupOverloadDevice(1, 1);

    // Reset Boss AI variables
    m_bossState = BossState::Normal;
    m_bossHealth = 100.0f;
    m_bossMaxHealth = 100.0f;
    m_bossAttackTimer = 0.0f;
    m_bossDroneSummonTimer = 0.0f;
    m_bossSweepTimer = 0.0f;
    m_slamGestureTimer = 0.0f;
    m_bossFlipped = false;
    m_bossProjectiles.clear();

    m_pulseLock.active = false;
    m_pulseLockCooldownTimer = 0.0f;
    m_weakenedTimer = 0.0f;

    // Load Sprites
    m_overloadDeviceSprite = std::make_unique<Background>();
    m_overloadDeviceSprite->Initialize("Asset/Train/PulseBox.png");

    m_pulseMarkSprite = std::make_unique<Background>();
    m_pulseMarkSprite->Initialize("Asset/Train/PulseMark.png");

    m_bossDroneProjectileSprite = std::make_unique<Background>();
    m_bossDroneProjectileSprite->Initialize("Asset/RedDrone.png");

    m_pulseLineH = std::make_unique<Background>();
    m_pulseLineH->Initialize("Asset/pulse/pulse_line_h.png");
    m_pulseLineV = std::make_unique<Background>();
    m_pulseLineV->Initialize("Asset/pulse/pulse_line_v.png");
    m_pulseCornerNE = std::make_unique<Background>();
    m_pulseCornerNE->Initialize("Asset/pulse/pulse_corner_ne.png");
    m_pulseCornerNW = std::make_unique<Background>();
    m_pulseCornerNW->Initialize("Asset/pulse/pulse_corner_nw.png");
    m_pulseCornerSE = std::make_unique<Background>();
    m_pulseCornerSE->Initialize("Asset/pulse/pulse_corner_se.png");
    m_pulseCornerSW = std::make_unique<Background>();
    m_pulseCornerSW->Initialize("Asset/pulse/pulse_corner_sw.png");

    m_cameraShakeRequest = 0.0f;
    m_nextVentIndex = -1;
}

void Final::Reset()
{
    // Reset Overload Devices
    m_overloadDevices[0].charge = 0.0f;
    m_overloadDevices[0].isOverloaded = false;
    m_overloadDevices[0].pulseAttackActive = false;
    m_overloadDevices[0].pulseAttackTimer = 0.0f;

    m_overloadDevices[1].charge = 0.0f;
    m_overloadDevices[1].isOverloaded = false;
    m_overloadDevices[1].pulseAttackActive = false;
    m_overloadDevices[1].pulseAttackTimer = 0.0f;

    m_cameraShakeRequest = 0.0f;

    // Reset Boss AI variables
    m_bossState = BossState::Normal;
    m_bossHealth = m_bossMaxHealth;
    m_bossAttackTimer = 0.0f;
    m_bossDroneSummonTimer = 0.0f;
    m_bossSweepTimer = 0.0f;
    m_slamGestureTimer = 0.0f;
    m_bossFlipped = false;
    m_bossProjectiles.clear();
    m_sweeps.clear();

    // Reset Boss NPC properties
    m_boss.Init({ Final::MIN_X + 3960.0f + 1436.0f, 0.0f });
    m_boss.SetSizeScale(1.0f);
    float bossY = (Final::MIN_Y + 258.0f) + m_boss.GetHitboxSize().y * 0.5f;
    m_boss.SetPosition({ Final::MIN_X + 3960.0f + 1436.0f, bossY });
    m_boss.SetCurrentGroundLevel(Final::MIN_Y + 258.0f);
    m_boss.ResetVelocity();
    m_boss.SetOnGround(true);

    // Restore hitboxes (re-enable exit gate hitbox at index 2 if it was cleared)
    if (m_hitboxes.size() > 2)
    {
        float w1 = m_final1->GetWidth() > 0 ? static_cast<float>(m_final1->GetWidth()) : 3960.0f;
        float pw = 348.0f;
        float ph = 662.0f;
        float cx = MIN_X + w1 + 3507.0f + pw * 0.5f;
        float cy = MIN_Y + (HEIGHT - 825.0f + ph * 0.5f);
        m_hitboxes[2].size = { pw, ph };
        m_hitboxes[2].pos = { cx, cy };
    }

    // Reset vent variables
    m_activeVentIndex = rand() % 3;
    for (auto& src : m_pulseSources)
    {
        src.SetPulseAmount(0.0f);
    }
    m_pulseSources[m_activeVentIndex].RefillStock();
    m_nextVentIndex = -1;
    m_ventTimer = 0.0f;

    m_pulseLock.active = false;
    m_pulseLockCooldownTimer = 0.0f;
    m_weakenedTimer = 0.0f;
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

void Final::Update(double dt, Player& player, Math::Vec2 playerHitboxSize, DroneManager& droneManager)
{
    float fdt = static_cast<float>(dt);
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
    m_sweepSpawnTimer += fdt;
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
        it->x += it->speed * fdt;

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
    m_ventTimer += fdt;
    if (m_ventTimer >= VENT_CYCLE_DURATION)
    {
        m_ventTimer = 0.0f;
        if (m_nextVentIndex != -1)
        {
            m_activeVentIndex = m_nextVentIndex;
            m_nextVentIndex = -1;
        }
        else
        {
            m_activeVentIndex = rand() % 3;
        }
        m_pulseSources[m_activeVentIndex].RefillStock();
    }
    else if (m_ventTimer >= VENT_ACTIVE_DURATION)
    {
        if (m_activeVentIndex != -1 && m_activeVentIndex < 3)
            m_pulseSources[m_activeVentIndex].SetPulseAmount(0.0f);
        m_activeVentIndex = -1;

        if (m_nextVentIndex == -1)
        {
            m_nextVentIndex = rand() % 3;
        }
    }

    // ----------------------------------------------------
    // Boss Physics & AI Logic
    // ----------------------------------------------------
    Math::Vec2 bossPos = m_boss.GetPosition();
    Math::Vec2 bossVel = m_boss.GetVelocity();
    bool bossOnGround = m_boss.IsOnGround();

    // Gravity
    if (!bossOnGround)
    {
        bossVel.y += -1500.0f * fdt;
    }
    else
    {
        bossVel.y = 0.0f;
    }

    // Boss State Machine
    if (m_bossState == BossState::Weakened)
    {
        bossVel.x = 0.0f;
        m_boss.UpdateNPC(fdt, AnimationState::Crouching);

        m_weakenedTimer += fdt;
        if (m_weakenedTimer >= WEAKENED_DURATION)
        {
            m_weakenedTimer = 0.0f;
            m_bossState = BossState::Normal;
            m_overloadDevices[0].charge = 0.0f;
            m_overloadDevices[0].isOverloaded = false;
            m_overloadDevices[1].charge = 0.0f;
            m_overloadDevices[1].isOverloaded = false;
        }

        if (m_bossHealth <= 0.0f)
        {
            m_bossHealth = 0.0f;
            m_bossState = BossState::Defeated;
            // Clear exit gate hitbox (index 2)
            if (m_hitboxes.size() > 2)
            {
                m_hitboxes[2].size = { 0.0f, 0.0f };
            }
        }
    }
    else if (m_bossState == BossState::Defeated)
    {
        bossVel = { 0.0f, 0.0f };
        bossPos.y -= 300.0f * fdt; // fall off screen
        m_boss.UpdateNPC(fdt, AnimationState::Idle);
    }
    else // Normal state AI
    {
        // 1. Gesture action
        if (m_slamGestureTimer > 0.0f)
        {
            m_slamGestureTimer -= fdt;
            bossVel.x = 0.0f;
            m_boss.UpdateNPC(fdt, AnimationState::Crouching);
            if (m_slamGestureTimer <= 0.0f)
            {
                // Slam shockwave sweep towards player
                FloorSweep sweep;
                sweep.width = 240.0f;
                sweep.damagedPlayer = false;
                sweep.x = bossPos.x;
                sweep.speed = (player.GetPosition().x > bossPos.x ? 450.0f : -450.0f);
                m_sweeps.push_back(sweep);
            }
        }
        else
        {
            // 2. Target decision: compete for active pulse vent or track player
            float targetX = player.GetPosition().x;
            if (m_activeVentIndex != -1)
            {
                targetX = m_hitboxes[3 + m_activeVentIndex].pos.x;

                // If boss reaches the vent, drain its stock
                if (std::abs(bossPos.x - targetX) < 100.0f && bossOnGround)
                {
                    m_pulseSources[m_activeVentIndex].Drain(25.0f * fdt); // Drains 25 units per second and automatically unlocks the remaining gauge representation

                    // If depleted, close the vent
                    if (m_pulseSources[m_activeVentIndex].GetPulseAmount() <= 0.0f)
                    {
                        m_activeVentIndex = -1;
                    }
                }
            }

            // 3. Horizontal movement direction
            float currentMoveSpeed = 220.0f;
            // Catch up speed (dashing) if player is far away
            float distToPlayer = std::abs(player.GetPosition().x - bossPos.x);
            if (distToPlayer > 600.0f)
            {
                currentMoveSpeed = 550.0f;
            }

            if (bossPos.x < targetX - 70.0f)
            {
                bossVel.x = currentMoveSpeed;
                m_bossFlipped = false;
                m_boss.UpdateNPC(fdt, AnimationState::Walking);
            }
            else if (bossPos.x > targetX + 70.0f)
            {
                bossVel.x = -currentMoveSpeed;
                m_bossFlipped = true;
                m_boss.UpdateNPC(fdt, AnimationState::Walking);
            }
            else
            {
                bossVel.x = 0.0f;
                m_boss.UpdateNPC(fdt, AnimationState::Idle);
            }

            m_boss.SetFlipped(m_bossFlipped);

            // 4. Dodge floor sweeps
            if (bossOnGround)
            {
                for (const auto& sweep : m_sweeps)
                {
                    float distToSweep = std::abs(sweep.x - bossPos.x);
                    // If sweep is close and moving towards boss
                    bool sweepApproaching = (sweep.speed > 0.0f && sweep.x < bossPos.x) || (sweep.speed < 0.0f && sweep.x > bossPos.x);
                    if (distToSweep < 280.0f && sweepApproaching)
                    {
                        // Jump over sweep
                        bossVel.y = 850.0f;
                        bossOnGround = false;
                        m_boss.SetOnGround(false);
                        break;
                    }
                }
            }

            // 5. Jump to follow player on higher platforms
            if (bossOnGround && player.GetPosition().y > bossPos.y + 150.0f && distToPlayer < 250.0f)
            {
                bossVel.y = 850.0f;
                bossOnGround = false;
                m_boss.SetOnGround(false);
            }

            // 6. Attacks
            // Ground Slam trigger
            m_bossSweepTimer += fdt;
            if (m_bossSweepTimer >= 6.5f)
            {
                m_slamGestureTimer = 0.5f;
                m_bossSweepTimer = 0.0f;
            }

            // Projectile Shoot trigger
            m_bossAttackTimer += fdt;
            if (m_bossAttackTimer >= 2.2f)
            {
                m_bossAttackTimer = 0.0f;
                Math::Vec2 targetCenter = player.GetHitboxCenter();
                Math::Vec2 dir = (targetCenter - bossPos).GetNormalized();
                BossProjectile proj;
                proj.pos = bossPos;
                proj.vel = dir * 650.0f;
                proj.active = true;
                m_bossProjectiles.push_back(proj);
            }

            // Drone Summon trigger (aggressive summoning)
            m_bossDroneSummonTimer += fdt;
            if (m_bossDroneSummonTimer >= 6.0f)
            {
                int activeDrones = 0;
                for (const auto& d : droneManager.GetDrones())
                {
                    if (!d.IsDead())
                    {
                        activeDrones++;
                    }
                }
                if (activeDrones < 3)
                {
                    m_bossDroneSummonTimer = 0.0f;
                    float summonX = player.GetPosition().x;
                    float summonY = (Final::MIN_Y + 258.0f) + 150.0f; // Lower flight height (150px above deck)

                    int choice = rand() % 4;
                    if (choice == 0) // Guard Device 0
                    {
                        summonX = m_overloadDevices[0].pos.x + (rand() % 200 - 100);
                        droneManager.SpawnDrone({ summonX, summonY }, "Asset/Drone.png", DroneType::General);
                    }
                    else if (choice == 1) // Guard Device 1
                    {
                        summonX = m_overloadDevices[1].pos.x + (rand() % 200 - 100);
                        droneManager.SpawnDrone({ summonX, summonY }, "Asset/Drone.png", DroneType::General);
                    }
                    else if (choice == 2 && m_activeVentIndex != -1) // Guard Active Vent
                    {
                        summonX = m_hitboxes[3 + m_activeVentIndex].pos.x + (rand() % 100 - 50);
                        droneManager.SpawnDrone({ summonX, summonY }, "Asset/Drone.png", DroneType::General);
                    }
                    else // Chase player directly (Tracer)
                    {
                        summonX = player.GetPosition().x + (rand() % 300 - 150);
                        droneManager.SpawnDrone({ summonX, summonY }, "Asset/RedDrone.png", DroneType::Tracer);
                    }
                }
            }

            // Pulse Lock trigger (every 5.0 seconds)
            m_pulseLockCooldownTimer -= fdt;
            if (m_pulseLockCooldownTimer <= 0.0f && !m_pulseLock.active)
            {
                m_pulseLock.targetPos = player.GetPosition();
                m_pulseLock.active = true;
                m_pulseLock.warningTimer = 0.0f;
                m_pulseLock.triggeredExplosion = false;
                m_pulseLock.explosionVisualTimer = 0.0f;
                m_pulseLockCooldownTimer = 5.0f;
            }
        }
    }

    // Update positions and velocities
    bossPos += bossVel * fdt;

    // Arena horizontal boundaries
    float minBossX = MIN_X + 50.0f;
    float maxBossX = MIN_X + m_mapWidth - 100.0f;
    if (bossPos.x < minBossX) { bossPos.x = minBossX; bossVel.x = 0.0f; }
    if (bossPos.x > maxBossX) { bossPos.x = maxBossX; bossVel.x = 0.0f; }

    // Platform ground collision
    float bossGroundLevel = Final::MIN_Y + 258.0f;
    float bossHalfH = m_boss.GetHitboxSize().y * 0.5f;
    if (bossPos.y - bossHalfH <= bossGroundLevel)
    {
        bossPos.y = bossGroundLevel + bossHalfH;
        if (bossVel.y < 0.0f)
        {
            bossVel.y = 0.0f;
        }
        bossOnGround = true;
        m_boss.SetOnGround(true);
    }
    else
    {
        bossOnGround = false;
        m_boss.SetOnGround(false);
    }

    m_boss.SetPosition(bossPos);
    m_boss.SetVelocity(bossVel);

    // ----------------------------------------------------
    // Update Boss Projectiles
    // ----------------------------------------------------
    for (auto& proj : m_bossProjectiles)
    {
        if (!proj.active) continue;

        proj.pos += proj.vel * fdt;

        // Out of bounds check
        if (proj.pos.x < MIN_X || proj.pos.x > MIN_X + m_mapWidth ||
            proj.pos.y < MIN_Y || proj.pos.y > MIN_Y + HEIGHT)
        {
            proj.active = false;
            continue;
        }

        // Collision with player
        bool hit = Collision::CheckPointInAABB(proj.pos, player.GetHitboxCenter(), player.GetHitboxSize());
        if (hit)
        {
            proj.active = false;
            if (!player.IsDead() && !player.IsGodMode())
            {
                player.TakeDamage(10.0f);
            }
        }
    }

    // Clean up inactive projectiles
    m_bossProjectiles.erase(
        std::remove_if(m_bossProjectiles.begin(), m_bossProjectiles.end(),
            [](const BossProjectile& p) { return !p.active; }),
        m_bossProjectiles.end());

    // ----------------------------------------------------
    // Update Pulse Lock Attack
    // ----------------------------------------------------
    if (m_pulseLock.active)
    {
        if (m_pulseLock.warningTimer < m_pulseLock.maxWarningTime)
        {
            m_pulseLock.warningTimer += fdt;
        }
        else if (!m_pulseLock.triggeredExplosion)
        {
            // Explosion triggers!
            m_pulseLock.triggeredExplosion = true;
            m_pulseLock.explosionVisualTimer = 0.4f;
            m_cameraShakeRequest = 18.0f; // Request screen shake with peak 18 pixels

            // Damage player if inside radius
            float dist = (player.GetHitboxCenter() - m_pulseLock.targetPos).Length();
            if (dist <= m_pulseLock.explosionRadius)
            {
                if (!player.IsDead() && !player.IsGodMode())
                {
                    player.TakeDamage(15.0f);
                }
            }
        }
        else
        {
            m_pulseLock.explosionVisualTimer -= fdt;
            if (m_pulseLock.explosionVisualTimer <= 0.0f)
            {
                m_pulseLock.active = false;
            }
        }
    }

    // ----------------------------------------------------
    // Update Overload Device Pulse Attacks
    // ----------------------------------------------------
    for (int i = 0; i < 2; ++i)
    {
        auto& dev = m_overloadDevices[i];
        if (dev.pulseAttackActive)
        {
            dev.pulseAttackTimer -= fdt;
            if (dev.pulseAttackTimer <= 0.0f)
            {
                dev.pulseAttackActive = false;
                dev.pulseAttackTimer = 0.0f;
            }
            else
            {
                // Deal damage if player is too close to the active pulse attack surrounding the device
                // The perimeter size is dev.size + Vec2(120.0f, 120.0f)
                Math::Vec2 range = dev.size + Math::Vec2(120.0f, 120.0f);
                if (Collision::CheckAABB(player.GetHitboxCenter(), playerHitboxSize, dev.pos, range))
                {
                    if (!player.IsDead() && !player.IsGodMode())
                    {
                        // Continuous damage: 12.0f units per second
                        player.TakeDamage(12.0f * fdt);
                    }
                }
            }
        }
    }
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

    // 1. Draw Overload Devices
    for (int i = 0; i < 2; ++i)
    {
        const auto& dev = m_overloadDevices[i];

        // 2. Draw vertical charge gauge next to the device if charging
        if (dev.charge > 0.0f && !dev.isOverloaded)
        {
            colorShader.use();
            colorShader.setMat4("projection", projection);

            float barW = 14.0f;
            float barH = 200.0f;
            float barX = dev.pos.x + dev.size.x * 0.5f + 30.0f;
            float barY = dev.pos.y;

            // Background
            DrawFilledQuad(colorShader, { barX, barY }, { barW + 6.0f, barH + 6.0f }, 0.2f, 0.2f, 0.2f, 1.0f);
            DrawFilledQuad(colorShader, { barX, barY }, { barW, barH }, 0.1f, 0.1f, 0.12f, 1.0f);

            // Fill (bright orange/red)
            float fillH = barH * dev.charge;
            if (fillH > 0.0f)
            {
                float fillY = (barY - barH * 0.5f) + fillH * 0.5f;
                DrawFilledQuad(colorShader, { barX, fillY }, { barW, fillH }, 1.0f, 0.4f, 0.1f, 1.0f);
            }
            
            shader.use();
            shader.setMat4("projection", projection);
        }

        // Draw device pulse attack perimeter if active
        if (dev.pulseAttackActive)
        {
            shader.use();
            shader.setMat4("projection", projection);
            shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
            shader.setBool("flipX", false);

            float time = static_cast<float>(glfwGetTime());
            // Pulsating intensity to look dynamic
            float pulseAlpha = 0.6f + 0.4f * std::sin(time * 15.0f);
            shader.setFloat("alpha", pulseAlpha);
            shader.setVec3("colorTint", 1.0f, 0.2f, 0.2f); // Hot neon red glow
            shader.setFloat("tintStrength", 1.0f);

            float borderW = dev.size.x + 120.0f;
            float borderH = dev.size.y + 120.0f;
            float tileS = 32.0f;

            Math::Vec2 halfB = { borderW * 0.5f, borderH * 0.5f };

            // 1. Draw Corners
            if (m_pulseCornerNW)
                m_pulseCornerNW->Draw(shader, Math::Matrix::CreateTranslation(dev.pos + Math::Vec2(-halfB.x, halfB.y)) * Math::Matrix::CreateScale({ tileS, tileS }));
            if (m_pulseCornerNE)
                m_pulseCornerNE->Draw(shader, Math::Matrix::CreateTranslation(dev.pos + Math::Vec2(halfB.x, halfB.y)) * Math::Matrix::CreateScale({ tileS, tileS }));
            if (m_pulseCornerSW)
                m_pulseCornerSW->Draw(shader, Math::Matrix::CreateTranslation(dev.pos + Math::Vec2(-halfB.x, -halfB.y)) * Math::Matrix::CreateScale({ tileS, tileS }));
            if (m_pulseCornerSE)
                m_pulseCornerSE->Draw(shader, Math::Matrix::CreateTranslation(dev.pos + Math::Vec2(halfB.x, -halfB.y)) * Math::Matrix::CreateScale({ tileS, tileS }));

            // 2. Draw Horizontal Lines (top and bottom)
            if (m_pulseLineH)
            {
                float startX = dev.pos.x - halfB.x + tileS;
                float endX = dev.pos.x + halfB.x - tileS;
                for (float tx = startX; tx <= endX; tx += tileS)
                {
                    m_pulseLineH->Draw(shader, Math::Matrix::CreateTranslation({ tx, dev.pos.y + halfB.y }) * Math::Matrix::CreateScale({ tileS, tileS }));
                    m_pulseLineH->Draw(shader, Math::Matrix::CreateTranslation({ tx, dev.pos.y - halfB.y }) * Math::Matrix::CreateScale({ tileS, tileS }));
                }
            }

            // 3. Draw Vertical Lines (left and right)
            if (m_pulseLineV)
            {
                float startY = dev.pos.y - halfB.y + tileS;
                float endY = dev.pos.y + halfB.y - tileS;
                for (float ty = startY; ty <= endY; ty += tileS)
                {
                    m_pulseLineV->Draw(shader, Math::Matrix::CreateTranslation({ dev.pos.x - halfB.x, ty }) * Math::Matrix::CreateScale({ tileS, tileS }));
                    m_pulseLineV->Draw(shader, Math::Matrix::CreateTranslation({ dev.pos.x + halfB.x, ty }) * Math::Matrix::CreateScale({ tileS, tileS }));
                }
            }

            // Reset tint state
            shader.setFloat("tintStrength", 0.0f);
            shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
            shader.setFloat("alpha", 1.0f);
        }
    }

    // 3. Draw Boss Projectiles (Flying Red Drones)
    if (!m_bossProjectiles.empty() && m_bossDroneProjectileSprite && m_bossDroneProjectileSprite->GetWidth() > 0)
    {
        shader.use();
        shader.setMat4("projection", projection);
        shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        shader.setFloat("alpha", 1.0f);
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        shader.setFloat("tintStrength", 0.0f);

        for (const auto& proj : m_bossProjectiles)
        {
            if (!proj.active) continue;

            // Flip the drone horizontally depending on its flight direction (facing the player)
            bool flipX = (proj.vel.x < 0.0f);
            shader.setBool("flipX", flipX);

            // Subtle rotation / tilt based on time to look like hovering/flying
            float tilt = std::sin(static_cast<float>(glfwGetTime()) * 15.0f) * 0.08f;
            
            float droneSize = 48.0f; // slightly larger than plain hitbox for visual appeal
            Math::Matrix model = Math::Matrix::CreateScale({ droneSize, droneSize })
                               * Math::Matrix::CreateRotation(tilt * (180.0f / 3.14159265f))
                               * Math::Matrix::CreateTranslation(proj.pos);

            m_bossDroneProjectileSprite->Draw(shader, model);
        }

        shader.setBool("flipX", false);
    }

    // 4. Draw Target Pulse Mark in Weakened state
    if (m_bossState == BossState::Weakened && m_pulseMarkSprite && m_pulseMarkSprite->GetWidth() > 0)
    {
        shader.use();
        shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        shader.setBool("flipX", false);
        shader.setFloat("alpha", 1.0f);
        shader.setVec3("colorTint", 1.0f, 0.1f, 0.1f);
        shader.setFloat("tintStrength", 1.0f);

        Math::Vec2 targetMarkPos = m_boss.GetPosition() + Math::Vec2(0.0f, m_boss.GetHitboxSize().y * 0.5f + 40.0f);
        float angleDegrees = static_cast<float>(glfwGetTime()) * 3.5f * (180.0f / 3.14159265f);
        Math::Matrix model = Math::Matrix::CreateScale({ 75.0f, 75.0f })
                           * Math::Matrix::CreateRotation(angleDegrees)
                           * Math::Matrix::CreateTranslation(targetMarkPos);
        m_pulseMarkSprite->Draw(shader, model);
    }

    // 4.5. Draw Pulse Lock Warning or Explosion
    if (m_pulseLock.active)
    {
        if (!m_pulseLock.triggeredExplosion && m_pulseMarkSprite && m_pulseMarkSprite->GetWidth() > 0)
        {
            shader.use();
            shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
            shader.setBool("flipX", false);

            float time = static_cast<float>(glfwGetTime());
            float progress = m_pulseLock.warningTimer / m_pulseLock.maxWarningTime;

            // 1. Outer Danger boundary (Blinking red, pulsating size slightly)
            float alphaOuter = 0.3f + 0.3f * std::sin(time * 12.0f);
            float pulseScale = 1.0f + 0.08f * std::sin(time * 12.0f);
            shader.setFloat("alpha", alphaOuter);
            shader.setVec3("colorTint", 1.0f, 0.1f, 0.1f);
            shader.setFloat("tintStrength", 0.9f);

            float angleDegreesOuter = time * 2.0f * (180.0f / 3.14159265f);
            Math::Vec2 szOuter = { m_pulseLock.explosionRadius * 2.0f * pulseScale, m_pulseLock.explosionRadius * 2.0f * pulseScale };
            Math::Matrix modelOuter = Math::Matrix::CreateScale(szOuter)
                                    * Math::Matrix::CreateRotation(angleDegreesOuter)
                                    * Math::Matrix::CreateTranslation(m_pulseLock.targetPos);
            m_pulseMarkSprite->Draw(shader, modelOuter);

            // 2. Inner Lock-on Reticle (Shrinks from 2.5x to 1.0x, fades color from hot orange to neon-yellow)
            float shrinkFactor = 2.5f - 1.5f * progress;
            float alphaInner = 0.5f + 0.5f * progress;
            shader.setFloat("alpha", alphaInner);
            shader.setVec3("colorTint", 1.0f, 0.5f + 0.5f * progress, 0.0f);
            shader.setFloat("tintStrength", 1.0f);

            float angleDegreesInner = -time * 5.0f * (180.0f / 3.14159265f);
            Math::Vec2 szInner = { m_pulseLock.explosionRadius * 2.0f * shrinkFactor, m_pulseLock.explosionRadius * 2.0f * shrinkFactor };
            Math::Matrix modelInner = Math::Matrix::CreateScale(szInner)
                                    * Math::Matrix::CreateRotation(angleDegreesInner)
                                    * Math::Matrix::CreateTranslation(m_pulseLock.targetPos);
            m_pulseMarkSprite->Draw(shader, modelInner);

            // Reset shader properties
            shader.setFloat("alpha", 1.0f);
            shader.setFloat("tintStrength", 0.0f);
            shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        }
        else if (m_pulseLock.triggeredExplosion && m_pulseMarkSprite && m_pulseMarkSprite->GetWidth() > 0)
        {
            shader.use();
            shader.setMat4("projection", projection);
            shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
            shader.setBool("flipX", false);

            float visualRatio = m_pulseLock.explosionVisualTimer / 0.4f; // fades from 1.0 to 0.0
            if (visualRatio > 0.0f)
            {
                // Expands from 0.8x to 1.8x size as it fades
                float scaleFactor = 0.8f + 1.0f * (1.0f - visualRatio);
                float size = m_pulseLock.explosionRadius * 2.0f * scaleFactor;
                float alpha = visualRatio * 0.9f;

                // Outer spin layer (clockwise, bright orange)
                float angleDegrees1 = static_cast<float>(glfwGetTime()) * 8.0f * (180.0f / 3.14159265f);
                shader.setFloat("alpha", alpha);
                shader.setVec3("colorTint", 1.0f, 0.45f, 0.0f);
                shader.setFloat("tintStrength", 1.0f);
                
                Math::Matrix model1 = Math::Matrix::CreateScale({ size, size })
                                    * Math::Matrix::CreateRotation(angleDegrees1)
                                    * Math::Matrix::CreateTranslation(m_pulseLock.targetPos);
                m_pulseMarkSprite->Draw(shader, model1);

                // Inner spin layer (counter-clockwise, slightly smaller, bright neon yellow)
                float angleDegrees2 = -static_cast<float>(glfwGetTime()) * 12.0f * (180.0f / 3.14159265f);
                shader.setFloat("alpha", alpha * 0.8f);
                shader.setVec3("colorTint", 1.0f, 0.85f, 0.1f);
                shader.setFloat("tintStrength", 1.0f);

                Math::Matrix model2 = Math::Matrix::CreateScale({ size * 0.75f, size * 0.75f })
                                    * Math::Matrix::CreateRotation(angleDegrees2)
                                    * Math::Matrix::CreateTranslation(m_pulseLock.targetPos);
                m_pulseMarkSprite->Draw(shader, model2);
            }

            // Reset shader properties
            shader.setFloat("alpha", 1.0f);
            shader.setFloat("tintStrength", 0.0f);
            shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        }
    }

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
    // 1. Draw pre-vent warning pulse mark if in warning/inactive phase
    if (m_nextVentIndex != -1 && m_ventTimer >= VENT_ACTIVE_DURATION && m_pulseMarkSprite && m_pulseMarkSprite->GetWidth() > 0)
    {
        const auto& hb = m_hitboxes[3 + m_nextVentIndex];

        float time = static_cast<float>(glfwGetTime());
        // Pulsate scale and blink alpha
        float pulseScale = 1.0f + 0.15f * std::sin(time * 8.0f);
        float warningAlpha = 0.5f + 0.5f * std::sin(time * 8.0f);

        shader.use();
        shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        shader.setBool("flipX", false);
        shader.setFloat("alpha", warningAlpha);
        shader.setVec3("colorTint", 1.0f, 0.3f, 0.3f); // reddish warning tint
        shader.setFloat("tintStrength", 0.5f);

        Math::Vec2 markSize = { 48.0f * pulseScale, 48.0f * pulseScale };
        float yPos = hb.pos.y - 90.0f + 24.0f; // sit near ground level

        // Draw left of the platform
        Math::Vec2 leftPos = { hb.pos.x - 120.0f, yPos };
        Math::Matrix modelLeft = Math::Matrix::CreateTranslation(leftPos) * Math::Matrix::CreateScale(markSize);
        m_pulseMarkSprite->Draw(shader, modelLeft);

        // Draw right of the platform
        Math::Vec2 rightPos = { hb.pos.x + 120.0f, yPos };
        Math::Matrix modelRight = Math::Matrix::CreateTranslation(rightPos) * Math::Matrix::CreateScale(markSize);
        m_pulseMarkSprite->Draw(shader, modelRight);

        // Reset tint for subsequent drawings
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        shader.setFloat("tintStrength", 0.0f);
        shader.setFloat("alpha", 1.0f);
    }

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

    // 3. Draw interior purple line fill (additive blend over sprite body)
    {
        float pulse = (std::sin(static_cast<float>(glfwGetTime()) * 4.0f) * 0.5f + 0.5f); // 0~1 pulsing
        float purpleAlpha = 0.35f + pulse * 0.25f;

        GL::Enable(GL_BLEND);
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE); // additive for glow effect
        shader.use();
        shader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);
        shader.setBool("flipX", false);
        shader.setFloat("alpha", purpleAlpha);
        shader.setVec3("colorTint", 0.55f, 0.0f, 1.0f); // purple
        shader.setFloat("tintStrength", 0.9f);
        m_pulseVentSprite->Draw(shader, model);

        // restore standard blend & tint
        GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        shader.setFloat("alpha", 1.0f);
        shader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
        shader.setFloat("tintStrength", 0.0f);
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
    if (m_overloadDeviceSprite)
    {
        m_overloadDeviceSprite->Shutdown();
        m_overloadDeviceSprite.reset();
    }
    if (m_pulseMarkSprite)
    {
        m_pulseMarkSprite->Shutdown();
        m_pulseMarkSprite.reset();
    }
    if (m_bossDroneProjectileSprite)
    {
        m_bossDroneProjectileSprite->Shutdown();
        m_bossDroneProjectileSprite.reset();
    }
    if (m_pulseLineH) { m_pulseLineH->Shutdown(); m_pulseLineH.reset(); }
    if (m_pulseLineV) { m_pulseLineV->Shutdown(); m_pulseLineV.reset(); }
    if (m_pulseCornerNE) { m_pulseCornerNE->Shutdown(); m_pulseCornerNE.reset(); }
    if (m_pulseCornerNW) { m_pulseCornerNW->Shutdown(); m_pulseCornerNW.reset(); }
    if (m_pulseCornerSE) { m_pulseCornerSE->Shutdown(); m_pulseCornerSE.reset(); }
    if (m_pulseCornerSW) { m_pulseCornerSW->Shutdown(); m_pulseCornerSW.reset(); }
    for (auto& src : m_pulseSources) src.Shutdown();
    m_boss.Shutdown();
}

void Final::DamageBoss(float amount)
{
    if (m_bossState == BossState::Weakened)
    {
        m_bossHealth -= amount;
        if (m_bossHealth <= 0.0f)
        {
            m_bossHealth = 0.0f;
            m_bossState = BossState::Defeated;
            // Clear exit gate hitbox (index 2)
            if (m_hitboxes.size() > 2)
            {
                m_hitboxes[2].size = { 0.0f, 0.0f };
            }
        }
    }
}

bool Final::IsDeviceHovered(int idx, Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld) const
{
    if (idx < 0 || idx >= 2 || m_overloadDevices[idx].isOverloaded)
        return false;

    const auto& dev = m_overloadDevices[idx];
    const Math::Vec2 range = { dev.size.x + 250.0f, dev.size.y + 200.0f };
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, dev.pos, range))
        return false;

    const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };
    return Collision::CheckPointInAABB(mouseWorld, dev.pos, dev.size) ||
           Collision::CheckAABB(mouseWorld, cursorHitbox, dev.pos, dev.size);
}

void Final::UpdateDeviceInject(int idx, float dt, Player& player, bool godMode)
{
    if (idx < 0 || idx >= 2 || m_overloadDevices[idx].isOverloaded)
        return;

    float pulsePerSec = 25.0f; // drains 25 units per second (takes 4 seconds of continuous hold)
    float pulseThisFrame = pulsePerSec * dt;

    if (!godMode)
    {
        pulseThisFrame = std::min(pulseThisFrame, player.GetPulseCore().getPulse().Value());
        if (pulseThisFrame > 0.0f)
        {
            player.GetPulseCore().getPulse().spend(pulseThisFrame);
        }
    }

    if (pulseThisFrame > 0.0f || godMode)
    {
        // Activate device pulse attack surrounding the device!
        m_overloadDevices[idx].pulseAttackActive = true;
        m_overloadDevices[idx].pulseAttackTimer = 2.0f; // refreshes as long as player continues injecting

        // Device Interference: if player is injecting, target them with a pulse lock attack on the player's position!
        if (!m_pulseLock.active && m_pulseLockCooldownTimer <= 0.0f)
        {
            m_pulseLock.targetPos = player.GetPosition(); // Target the player's position directly
            m_pulseLock.active = true;
            m_pulseLock.warningTimer = 0.0f;
            m_pulseLock.triggeredExplosion = false;
            m_pulseLock.explosionVisualTimer = 0.0f;
            m_pulseLockCooldownTimer = 2.5f; // shorter cooldown during interference to force movement
        }

        float deltaCharge = godMode ? (dt / 3.0f) : (pulseThisFrame / 100.0f); // 100 units to fully charge
        m_overloadDevices[idx].charge += deltaCharge;

        if (m_overloadDevices[idx].charge >= 1.0f)
        {
            m_overloadDevices[idx].charge = 1.0f;
            m_overloadDevices[idx].isOverloaded = true;
            m_overloadDevices[idx].pulseAttackActive = false;
            m_overloadDevices[idx].pulseAttackTimer = 0.0f;
        }
    }

    // Check if both devices are overloaded
    if (m_overloadDevices[0].isOverloaded && m_overloadDevices[1].isOverloaded)
    {
        if (m_bossState == BossState::Normal)
        {
            m_bossState = BossState::Weakened;
            m_bossHealth = 100.0f; // Set Weakened health
        }
    }
}

bool Final::IsBossHovered(Math::Vec2 mouseWorld) const
{
    if (m_bossState != BossState::Weakened)
        return false;

    const Math::Vec2 cursorHitbox = { 32.0f, 32.0f };
    Math::Vec2 bossPos = m_boss.GetPosition();
    Math::Vec2 bossSize = m_boss.GetHitboxSize();

    return Collision::CheckPointInAABB(mouseWorld, bossPos, bossSize) ||
           Collision::CheckAABB(mouseWorld, cursorHitbox, bossPos, bossSize);
}

float Final::ConsumeCameraShakeRequest()
{
    float shake = m_cameraShakeRequest;
    m_cameraShakeRequest = 0.0f;
    return shake;
}

