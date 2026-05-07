//PulseManager.cpp

#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PulseCore.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include <cmath>
#include <vector>
#include <algorithm>
#include <random> 

#pragma warning(push, 0)
#include <stb_image.h>
#pragma warning(pop)

constexpr float PI = 3.14159265359f;

std::default_random_engine generator;
std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

void PulseManager::Initialize()
{
    auto LoadTexture = [&](const char* path) -> GLuint
        {
            GLuint tex = 0;
            GL::GenTextures(1, &tex);
            GL::BindTexture(GL_TEXTURE_2D, tex);

            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            int w, h, ch;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* data = stbi_load(path, &w, &h, &ch, 0);

            if (!data)
            {
                Logger::Instance().Log(Logger::Severity::Error, "Failed to load texture: %s", path);
                return 0;
            }

            GLenum format = (ch == 4) ? GL_RGBA : GL_RGB;
            GL::TexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
            GL::GenerateMipmap(GL_TEXTURE_2D);

            stbi_image_free(data);
            return tex;
        };

    m_texCornerNE = LoadTexture("Asset/pulse/pulse_corner_ne.png");
    m_texCornerNW = LoadTexture("Asset/pulse/pulse_corner_nw.png");
    m_texCornerSE = LoadTexture("Asset/pulse/pulse_corner_se.png");
    m_texCornerSW = LoadTexture("Asset/pulse/pulse_corner_sw.png");
    m_texLineH = LoadTexture("Asset/pulse/pulse_line_h.png");
    m_texLineV = LoadTexture("Asset/pulse/pulse_line_v.png");

    m_fluidTexID = LoadTexture("Asset/pulse/PulseFluid.png");

    float vertices[] = {
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
        -0.5f, -0.5f,   0.0f, 0.0f,
        -0.5f,  0.5f,   0.0f, 1.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
         0.5f, -0.5f,   1.0f, 0.0f
    };

    GL::GenVertexArrays(1, &m_pulseVAO);
    GL::GenBuffers(1, &m_pulseVBO);
    GL::BindVertexArray(m_pulseVAO);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_pulseVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);
    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    GL::EnableVertexAttribArray(1);
    GL::BindVertexArray(0);

    m_logTimer = 0.0;
}



void PulseManager::Shutdown()
{
    if (m_fluidTexID != 0) GL::DeleteTextures(1, &m_fluidTexID);

    if (m_texCornerNE) GL::DeleteTextures(1, &m_texCornerNE);
    if (m_texCornerNW) GL::DeleteTextures(1, &m_texCornerNW);
    if (m_texCornerSE) GL::DeleteTextures(1, &m_texCornerSE);
    if (m_texCornerSW) GL::DeleteTextures(1, &m_texCornerSW);
    if (m_texLineH)    GL::DeleteTextures(1, &m_texLineH);
    if (m_texLineV)    GL::DeleteTextures(1, &m_texLineV);

    GL::DeleteVertexArrays(1, &m_pulseVAO);
    GL::DeleteBuffers(1, &m_pulseVBO);

    m_fluidTexID = 0;
    m_texCornerNE = m_texCornerNW = m_texCornerSE = m_texCornerSW = 0;
    m_texLineH = m_texLineV = 0;
}


void PulseManager::UpdateAttackVFX(
    bool isAttacking,
    Math::Vec2 startPos,
    Math::Vec2 endPos,
    float attackSide)
{
    if (!isAttacking)
    {
        m_isAttacking = false;
        m_attackPathValid = false;
        m_attackPathUpdateTimer = 0.0f;
        m_attackElapsed = 0.0f;
        m_attackPacketActive = false;
        return;
    }

    m_isAttacking = true;
    m_attackEndLive = endPos;

    float currentSide = (attackSide >= 0.0f) ? 1.0f : -1.0f;

    if (m_attackPathValid)
    {
        float oldSide = (m_attackPrevEnd.x - m_attackStartFrozen.x >= 0.0f) ? 1.0f : -1.0f;

        if (oldSide != currentSide)
        {
            m_attackStartFrozen = startPos;
            m_attackPrevEnd = endPos;
            m_attackPathUpdateTimer = 0.0f;

            m_attackElapsed = 0.0f;
            m_attackPacketActive = true;
        }
    }

    if (!m_attackPathValid)
    {
        m_attackStartFrozen = startPos;
        m_attackPrevEnd = endPos;
        m_attackPathValid = true;
        m_attackPathUpdateTimer = 0.0f;

        m_attackElapsed = 0.0f;
        m_attackPacketActive = true;
    }

    m_attackStartFrozen = startPos;
    m_attackPrevEnd = endPos;

    Math::Vec2 start = m_attackStartFrozen;
    Math::Vec2 end = m_attackEndLive;

    float dx = end.x - start.x;
    float signX = (attackSide >= 0.0f) ? 1.0f : -1.0f;

   /* const float turnStep = 48.0f * m_vfxScale;
    const float minLastLeg = 16.0f * m_vfxScale;

    float midX = start.x + signX * std::min(std::abs(dx) * 0.5f, turnStep);

    if (signX > 0.0f)
    {
        midX = std::min(midX, end.x - minLastLeg);
        midX = std::max(midX, start.x);
    }
    else
    {
        midX = std::max(midX, end.x + minLastLeg);
        midX = std::min(midX, start.x); 
    }*/
    float midX = start.x + dx * 0.5f;

    m_attackC1 = { midX, start.y };
    m_attackC2 = { midX, end.y };
    m_attackC3 = end;

    auto segLen = [&](Math::Vec2 a, Math::Vec2 b)
        {
            return (b - a).Length();
        };

    float L0 = segLen(start, m_attackC1);
    float L1 = segLen(m_attackC1, m_attackC2);
    float L2 = segLen(m_attackC2, m_attackC3);
    float L3 = 0.0f;

    m_attackTotalLen = L0 + L1 + L2 + L3;

    const float packetSpeed = 350.0f;
    float ideal = (m_attackTotalLen > 1.0f) ? (m_attackTotalLen / packetSpeed) : 0.25f;
    m_attackTravelTime = std::clamp(ideal, 0.35f, 0.75f);
}

void PulseManager::Update(Math::Vec2 playerHitboxCenter, Math::Vec2 playerHitboxSize,
    Player& player, std::vector<PulseSource>& roomSources,
    std::vector<PulseSource>& hallwaySources,
    std::vector<PulseSource>& rooftopSources,
    std::vector<PulseSource>& undergroundSources,
    std::vector<PulseSource>& trainSources,
    bool is_interact_key_pressed, double dt, Math::Vec2 mouseWorldPos)
{
    float fdt = static_cast<float>(dt);

    if (m_isAttacking && m_attackPathValid)
        m_attackPathUpdateTimer += fdt;

    if (m_isAttacking && m_attackPacketActive)
    {
        m_attackElapsed += fdt;

        if (m_attackElapsed >= m_attackTravelTime)
        {
            m_attackElapsed = m_attackTravelTime;
            m_attackPacketActive = false; 
        }
    }

    m_vfxTimer += fdt;
    m_logTimer += dt;

    if (m_detonationActive)
    {
        m_detonationTimer += fdt;
        if (m_detonationTimer >= DETONATION_TOTAL_DURATION)
            m_detonationActive = false;
    }

    // Chain arc timers
    for (auto& arc : m_chainArcs)
        arc.timer += fdt;
    m_chainArcs.erase(
        std::remove_if(m_chainArcs.begin(), m_chainArcs.end(),
            [](const ChainArc& a) { return a.timer >= CHAIN_ARC_DURATION; }),
        m_chainArcs.end());

    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;

    bool overlapAndCursorOnSource = false;

    auto checkSource = [&](PulseSource& source) {
        if (!source.HasPulse()) return;
        bool playerOverlaps = Collision::CheckAABB(playerHitboxCenter, playerHitboxSize, source.GetPosition(), source.GetHitboxSize());
        bool cursorOnSource = Collision::CheckPointInAABB(mouseWorldPos, source.GetPosition(), source.GetHitboxSize());
        if (playerOverlaps && cursorOnSource)
        {
            float dist_sq = (playerHitboxCenter - source.GetPosition()).LengthSq();
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
                overlapAndCursorOnSource = true;
            }
        }
        };

    for (auto& source : roomSources)       checkSource(source);
    for (auto& source : hallwaySources)    checkSource(source);
    for (auto& source : rooftopSources)    checkSource(source);
    for (auto& source : undergroundSources) checkSource(source);
    for (auto& source : trainSources)      checkSource(source);

    // Charging is allowed only when:
    // 1) player overlaps a pulse source and
    // 2) mouse cursor is on that pulse source.
    bool is_near_charger = (closest_source != nullptr && overlapAndCursorOnSource);
    PulseTickResult result = player.GetPulseCore().tick(is_interact_key_pressed, is_near_charger, false, dt);

    if (result.charged && closest_source != nullptr)
    {
        closest_source->Drain(result.delta);

        if (m_logTimer >= 2.0)
        {
            Logger::Instance().Log(Logger::Severity::Debug, "Charging pulse! Amount: %f", result.delta);
            m_logTimer -= 2.0;
        }

        m_isCharging = true;
        m_chargeStartPos = closest_source->GetPosition();
        m_chargeEndPos = playerHitboxCenter;
    }
    else
    {
        m_isCharging = false;
    }
}

void PulseManager::SetVFXScale(float scale)
{
    m_vfxScale = std::clamp(scale, 0.65f, 1.15f);
}

void PulseManager::DrawVFX(const Shader& shader) const
{
    if (!m_isAttacking && !m_isCharging) return;

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader.use();
    GL::ActiveTexture(GL_TEXTURE0);
    GL::BindVertexArray(m_pulseVAO);

    shader.setBool("flipX", false);
    shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);

    const float s = m_vfxScale;
    const float TILE = 16.0f * s;
    const float packetLen = 14.0f * s;
    const float packetGap = 7.0f * s;
    const float packetThickness = 6.0f * s;
    const float packetLen2Mul = 0.75f;
    const float packetThick2Mul = 0.85f;

    auto snapTile = [&](Math::Vec2 p) -> Math::Vec2
    {
        return p;
    };

    auto drawSprite = [&](Math::Vec2 p, Math::Vec2 size)
        {
            Math::Matrix model =
                Math::Matrix::CreateTranslation(p) *
                Math::Matrix::CreateScale(size);
            shader.setMat4("model", model);
            GL::DrawArrays(GL_TRIANGLES, 0, 6);
        };

    auto safeDir = [&](Math::Vec2 a, Math::Vec2 b) -> Math::Vec2
        {
            Math::Vec2 v = b - a;
            float l = v.Length();
            if (l < 0.0001f) return Math::Vec2{ 1.f, 0.f };
            return v / l;
        };

    auto cornerTexFromDirs = [&](Math::Vec2 inDir, Math::Vec2 outDir) -> GLuint
        {
            auto axis = [&](Math::Vec2 d) -> Math::Vec2
                {
                    if (std::abs(d.x) >= std::abs(d.y))
                        return { (d.x >= 0.f) ? 1.f : -1.f, 0.f };
                    else
                        return { 0.f, (d.y >= 0.f) ? 1.f : -1.f };
                };

            Math::Vec2 a = axis(inDir);
            Math::Vec2 b = axis(outDir);

            float hx = (a.x != 0.f) ? a.x : b.x; 
            float vy = (a.y != 0.f) ? a.y : b.y; 

            vy = -vy;

            if (hx > 0.f && vy > 0.f) return m_texCornerNE;
            if (hx < 0.f && vy > 0.f) return m_texCornerNW;
            if (hx > 0.f && vy < 0.f) return m_texCornerSE;
            if (hx < 0.f && vy < 0.f) return m_texCornerSW;

            return m_texCornerNE;
        };


    auto drawLineH_Tiled = [&](Math::Vec2 a, Math::Vec2 b)
        {
            a = snapTile(a); b = snapTile(b);
            if (b.x < a.x) std::swap(a, b);

            float len = b.x - a.x;
            if (len < 0.5f) return;

            GL::BindTexture(GL_TEXTURE_2D, m_texLineH);

            int full = (int)(len / TILE);
            float rem = len - full * TILE;

            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            for (int i = 0; i < full; ++i)
            {
                Math::Vec2 p = { a.x + (i + 0.5f) * TILE, a.y };
                drawSprite(p, { TILE, TILE });
            }

            if (rem > 0.5f)
            {
                float u = rem / TILE;
                shader.setVec4("spriteRect", 0.f, 0.f, u, 1.f);

                Math::Vec2 p = { a.x + full * TILE + rem * 0.5f, a.y };
                drawSprite(p, { rem, TILE });

                shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            }
        };

    auto drawLineV_Tiled = [&](Math::Vec2 a, Math::Vec2 b)
        {
            a = snapTile(a); b = snapTile(b);
            if (b.y < a.y) std::swap(a, b);

            float len = b.y - a.y;
            if (len < 0.5f) return;

            GL::BindTexture(GL_TEXTURE_2D, m_texLineV);

            int full = (int)(len / TILE);
            float rem = len - full * TILE;

            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            for (int i = 0; i < full; ++i)
            {
                Math::Vec2 p = { a.x, a.y + (i + 0.5f) * TILE };
                drawSprite(p, { TILE, TILE });
            }

            if (rem > 0.5f)
            {
                float v = rem / TILE;
                shader.setVec4("spriteRect", 0.f, 0.f, 1.f, v);

                Math::Vec2 p = { a.x, a.y + full * TILE + rem * 0.5f };
                drawSprite(p, { TILE, rem });

                shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            }
        };

    auto drawSegment_Tiled = [&](Math::Vec2 a, Math::Vec2 b)
        {
            Math::Vec2 d = b - a;
            if (std::abs(d.x) >= std::abs(d.y)) drawLineH_Tiled(a, b);
            else                                drawLineV_Tiled(a, b);
        };

    auto drawCornerAuto = [&](Math::Vec2 prev, Math::Vec2 corner, Math::Vec2 next)
        {
            prev = snapTile(prev);
            corner = snapTile(corner);
            next = snapTile(next);

            Math::Vec2 inDir = corner - prev;
            Math::Vec2 outDir = next - corner;

            GLuint tex = cornerTexFromDirs(inDir, outDir);
            GL::BindTexture(GL_TEXTURE_2D, tex);

            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            drawSprite(corner, { TILE, TILE });
        };

    if (m_isAttacking && m_attackPathValid)
    {
        Math::Vec2 p0 = snapTile(m_attackStartFrozen);
        Math::Vec2 p1 = snapTile(m_attackC1);
        Math::Vec2 p2 = snapTile(m_attackC2);
        Math::Vec2 p3 = snapTile(m_attackC3);
        Math::Vec2 p4 = snapTile(m_attackEndLive);

        drawSegment_Tiled(p0, p1);
        drawSegment_Tiled(p1, p2);
        drawSegment_Tiled(p2, p4);

        drawCornerAuto(p0, p1, p2);
        drawCornerAuto(p1, p2, p4);

        if (m_attackPacketActive)
        {
            auto segLen = [&](Math::Vec2 a, Math::Vec2 b) { return (b - a).Length(); };
            float L0 = segLen(p0, p1);
            float L1 = segLen(p1, p2);
            float L2 = segLen(p2, p3);
            float L3 = segLen(p3, p4);
            float totalLen = L0 + L1 + L2 + L3;

            if (totalLen >= 1.0f)
            {
                auto pointOnPath = [&](float s) -> Math::Vec2
                    {
                        if (s <= L0) return p0 + safeDir(p0, p1) * s; s -= L0;
                        if (s <= L1) return p1 + safeDir(p1, p2) * s; s -= L1;
                        if (s <= L2) return p2 + safeDir(p2, p3) * s; s -= L2;
                        if (s <= L3) return p3 + safeDir(p3, p4) * s;
                        return p4;
                    };

                auto dirOnPath = [&](float s) -> Math::Vec2
                    {
                        if (s <= L0) return safeDir(p0, p1); s -= L0;
                        if (s <= L1) return safeDir(p1, p2); s -= L1;
                        if (s <= L2) return safeDir(p2, p3); s -= L2;
                        return safeDir(p3, p4);
                    };

                float travelTime = std::max(m_attackTravelTime, 0.001f);
                float phase = std::clamp(m_attackElapsed / travelTime, 0.0f, 1.0f);

                if (phase < 1.0f)
                {
                    float headS = phase * totalLen;

                    GL::BindTexture(GL_TEXTURE_2D, m_fluidTexID);
                    shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);

                    auto drawPacketRect = [&](float sHead, float len, float thickness)
                        {
                            if (sHead < 0.0f || sHead > totalLen) return;

                            float sTail = sHead - len;
                            if (sTail < 0.0f) sTail = 0.0f;

                            Math::Vec2 headPos = pointOnPath(sHead);
                            Math::Vec2 tailPos = pointOnPath(sTail);
                            Math::Vec2 center = (headPos + tailPos) * 0.5f;

                            Math::Vec2 dir = dirOnPath(sHead);
                            float ax = std::abs(dir.x);
                            float ay = std::abs(dir.y);

                            float visualLen = (headPos - tailPos).Length();
                            if (visualLen < 0.5f) return;

                            // IMPORTANT: packets are simple quads, ok to scale freely
                            if (ax >= ay) drawSprite(center, { visualLen, thickness });
                            else          drawSprite(center, { thickness, visualLen });
                        };

                    drawPacketRect(headS, packetLen, packetThickness);

                    if (packetGap > 0.0f)
                    {
                        drawPacketRect(headS - packetGap, packetLen* packetLen2Mul, packetThickness* packetThick2Mul);
                    }
                }
            }
        }

        shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
        GL::BindVertexArray(0);
        GL::Disable(GL_BLEND);
        return;
    }

    if (m_isCharging)
    {
        Math::Vec2 start = snapTile(m_chargeStartPos);
        Math::Vec2 end = snapTile(m_chargeEndPos);
        Math::Vec2 diff = end - start;

        if (diff.Length() >= 1.0f)
        {
            Math::Vec2 cornerA = { end.x, start.y };
            Math::Vec2 cornerB = { start.x, end.y };

            float lenA = (cornerA - start).Length() + (end - cornerA).Length();
            float lenB = (cornerB - start).Length() + (end - cornerB).Length();
            Math::Vec2 corner = snapTile((lenA <= lenB) ? cornerA : cornerB);

            drawSegment_Tiled(start, corner);
            drawSegment_Tiled(corner, end);
            drawCornerAuto(start, corner, end);
        }

        shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
        GL::BindVertexArray(0);
        GL::Disable(GL_BLEND);
        return;
    }

    GL::BindVertexArray(0);
    GL::Disable(GL_BLEND);
}

void PulseManager::StartDetonationVFX(Math::Vec2 origin, float maxRadius,
    const std::vector<std::pair<Math::Vec2, Math::Vec2>>& chainArcs)
{
    m_detonationActive    = true;
    m_detonationOrigin    = origin;
    m_detonationMaxRadius = maxRadius;
    m_detonationTimer     = 0.f;

    m_chainArcs.clear();
    for (const auto& [from, to] : chainArcs)
        m_chainArcs.push_back({ from, to, 0.f });
}

void PulseManager::SyncDetonationOriginToPlayer(Math::Vec2 playerHitboxCenter)
{
    if (m_detonationActive)
        m_detonationOrigin = playerHitboxCenter;
}

void PulseManager::DrawDetonationVFX(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (!m_detonationActive && m_chainArcs.empty())
        return;

    // ── Shockwave ────────────────────────────────────────────────────────────
    if (m_detonationActive)
    {
        const float t = m_detonationTimer;

        // ① Center burst flash — 4 concentric rings expanding fast (t < 0.20s)
        {
            const float BURST_DUR = 0.20f;
            if (t < BURST_DUR)
            {
                float p = t / BURST_DUR;
                float fade = 1.f - p;
                for (int k = 0; k < 4; ++k)
                {
                    float kp = p * (1.f - k * 0.12f);
                    float r  = kp * m_detonationMaxRadius * 0.45f;
                    float f  = fade * (1.f - k * 0.2f);
                    debugRenderer.DrawCircle(colorShader, m_detonationOrigin, r,
                                             { f * 0.6f, f });
                }
            }
        }

        // ② Radial spike lines (t < 0.28s) — 12 spikes radiating outward
        {
            const float SPIKE_DUR = 0.28f;
            if (t < SPIKE_DUR)
            {
                float p    = t / SPIKE_DUR;
                float len  = p * m_detonationMaxRadius * 0.60f;
                float fade = 1.f - p * p;  // quad fade

                constexpr int SPIKE_COUNT = 12;
                for (int j = 0; j < SPIKE_COUNT; ++j)
                {
                    float angle = j * (2.f * PI / SPIKE_COUNT);
                    Math::Vec2 endPt = m_detonationOrigin
                        + Math::Vec2{ std::cos(angle), std::sin(angle) } * len;
                    float rS = fade * 0.3f, gS = fade, bS = fade;
                    debugRenderer.DrawLine(colorShader, m_detonationOrigin, endPt, rS, gS, bS, 7.f);

                    // Secondary spike at halfway angle
                    float aHalf = angle + (PI / SPIKE_COUNT);
                    Math::Vec2 ep2 = m_detonationOrigin
                        + Math::Vec2{ std::cos(aHalf), std::sin(aHalf) } * (len * 0.60f);
                    debugRenderer.DrawLine(colorShader, m_detonationOrigin, ep2,
                                           rS * 0.55f, gS * 0.55f, bS * 0.55f, 4.5f);
                }
            }
        }

        // ③ 5 expanding shockwave rings — eased out, each with inner ghost
        for (int i = 0; i < DETONATION_RING_COUNT; ++i)
        {
            float effT = t - i * DETONATION_RING_STAGGER;
            if (effT <= 0.f || effT >= DETONATION_RING_DURATION) continue;

            float progress = effT / DETONATION_RING_DURATION;
            float easedP   = 1.f - (1.f - progress) * (1.f - progress); // ease-out quad
            float eRadius  = easedP * m_detonationMaxRadius;
            float fade     = 1.f - progress;

            debugRenderer.DrawCircle(colorShader, m_detonationOrigin, eRadius,
                                     { fade * 0.15f, fade });
            if (eRadius > 8.f)
                debugRenderer.DrawCircle(colorShader, m_detonationOrigin, eRadius * 0.88f,
                                         { fade * 0.05f, fade * 0.55f });
            // Extra bright leading edge (slightly larger)
            debugRenderer.DrawCircle(colorShader, m_detonationOrigin, eRadius * 1.03f,
                                     { fade * 0.08f, fade * 0.70f });
        }
    }

    // ── Chain arc — 지지직 purple zigzag lightning ───────────────────────────
    for (const auto& arc : m_chainArcs)
    {
        float p    = arc.timer / CHAIN_ARC_DURATION;
        float fade = std::max(0.f, 1.f - p * 0.75f); // slower fade-out so arcs stay readable
        if (fade <= 0.f) continue;

        Math::Vec2 dir = arc.to - arc.from;
        float len = dir.Length();
        if (len < 1.f) continue;

        Math::Vec2 normDir = dir / len;
        Math::Vec2 perp    = normDir.Perpendicular();

        // 부드러운 밝기 변조 (지지직 단절 느낌 완화)
        const float flicker = 0.78f + 0.22f * (0.5f + 0.5f * std::sin(arc.timer * 22.f));
        float       ef      = fade * flicker;

        // 지그재그: 주파수 낮춤 → 흐름이 더 부드럽고 읽기 쉬움
        const float amp  = 44.f * fade;
        const float freq = 16.f;
        Math::Vec2 q1 = arc.from + normDir * (len * 0.25f)
                        + perp * (amp * std::sin(arc.timer * freq));
        Math::Vec2 q2 = arc.from + normDir * (len * 0.50f)
                        + perp * (amp * std::cos(arc.timer * freq + 1.3f));
        Math::Vec2 q3 = arc.from + normDir * (len * 0.75f)
                        + perp * (amp * std::sin(arc.timer * freq + 2.7f));

        auto seg = [&](Math::Vec2 a, Math::Vec2 b, float r, float g, float bcol, float thick) {
            debugRenderer.DrawLine(colorShader, a, b, r, g, bcol, thick);
        };

        // 넓은 페더 (가장 아래 — 차-차 / 차-드론 연쇄가 잘 보이도록)
        float rF = ef * 0.35f, gF = ef * 0.06f, bF = ef * 0.45f;
        seg(arc.from, q1, rF, gF, bF, 26.f);
        seg(q1, q2, rF, gF, bF, 26.f);
        seg(q2, q3, rF, gF, bF, 26.f);
        seg(q3, arc.to, rF, gF, bF, 26.f);

        // 직선 백본 (지그재그 중심 대략 따라 가시성 보강)
        float rB = ef * 0.45f, gB = ef * 0.08f, bB = ef * 0.55f;
        seg(arc.from, arc.to, rB, gB, bB, 12.f);

        // 코어: 밝은 지그재그
        float rC = ef * 1.00f, gC = ef * 0.20f, bC = ef * 1.00f;
        seg(arc.from, q1, rC, gC, bC, 9.f);
        seg(q1, q2, rC, gC, bC, 9.f);
        seg(q2, q3, rC, gC, bC, 9.f);
        seg(q3, arc.to, rC, gC, bC, 9.f);

        // 시안 하이라이트 (두껍게)
        {
            float rA = ef * 0.18f, gA = ef * 1.00f, bA = ef * 1.0f;
            constexpr float AC_OFF = 5.0f;
            Math::Vec2      aoff   = normDir * AC_OFF + perp * (AC_OFF * 0.45f);
            seg(arc.from + aoff * 0.25f, q2 + aoff, rA, gA, bA, 6.f);
            seg(q2 + aoff, arc.to - aoff * 0.18f, rA * 0.88f, gA * 0.88f, bA * 0.88f, 5.f);
        }

        // 글로우: 이중 오프셋 직선
        float rG = ef * 0.88f, gG = ef * 0.12f, bG = ef * 1.0f;
        constexpr float GLOW_OFF = 11.0f;
        Math::Vec2      off      = perp * GLOW_OFF;
        seg(arc.from + off, arc.to + off, rG * 0.55f, gG * 0.55f, bG * 0.55f, 8.f);
        seg(arc.from - off, arc.to - off, rG * 0.55f, gG * 0.55f, bG * 0.55f, 8.f);
        seg(arc.from + off * 1.55f, arc.to + off * 1.55f, rG * 0.40f, gG * 0.40f, bG * 0.40f, 5.f);
        seg(arc.from - off * 1.55f, arc.to - off * 1.55f, rG * 0.40f, gG * 0.40f, bG * 0.40f, 5.f);

        float hf   = fade * 1.0f;
        float hitR = 46.f + (1.f - fade) * 58.f;
        debugRenderer.DrawCircle(colorShader, arc.to, hitR, { hf * 0.90f, 0.f });
        debugRenderer.DrawCircle(colorShader, arc.to, hitR * 0.58f, { hf * 0.68f, 0.f });

        float sf   = fade * 0.78f;
        float srcR = 32.f + (1.f - fade) * 34.f;
        debugRenderer.DrawCircle(colorShader, arc.from, srcR, { sf * 0.75f, 0.f });
    }
}
