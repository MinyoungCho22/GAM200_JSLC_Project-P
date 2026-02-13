//PulseManager.cpp

#include "PulseManager.hpp"
#include "Player.hpp"
#include "PulseSource.hpp"
#include "../Game/PulseCore.hpp"
#include "../Engine/Logger.hpp"
#include "../Engine/Vec2.hpp"
#include "../Engine/Collision.hpp"
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
    if (m_circuitTexID != 0) GL::DeleteTextures(1, &m_circuitTexID);
    if (m_fluidTexID != 0) GL::DeleteTextures(1, &m_fluidTexID);

    if (m_texCornerNE) GL::DeleteTextures(1, &m_texCornerNE);
    if (m_texCornerNW) GL::DeleteTextures(1, &m_texCornerNW);
    if (m_texCornerSE) GL::DeleteTextures(1, &m_texCornerSE);
    if (m_texCornerSW) GL::DeleteTextures(1, &m_texCornerSW);
    if (m_texLineH)    GL::DeleteTextures(1, &m_texLineH);
    if (m_texLineV)    GL::DeleteTextures(1, &m_texLineV);

    GL::DeleteVertexArrays(1, &m_pulseVAO);
    GL::DeleteBuffers(1, &m_pulseVBO);

    m_circuitTexID = 0;
    m_fluidTexID = 0;
    m_texCornerNE = m_texCornerNW = m_texCornerSE = m_texCornerSW = 0;
    m_texLineH = m_texLineV = 0;
}


void PulseManager::UpdateAttackVFX(bool isAttacking, Math::Vec2 startPos, Math::Vec2 endPos)
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

    if (!m_attackPathValid)
    {
        m_attackStartFrozen = startPos;
        m_attackPrevEnd = endPos;
        m_attackPathValid = true;
        m_attackPathUpdateTimer = 0.0f;

        m_attackElapsed = 0.0f;
        m_attackPacketActive = true;
    }

    m_attackPrevEnd = endPos;

    const float UPDATE_INTERVAL = 0.15f;

    float moved = (startPos - m_attackStartFrozen).Length();
    bool shouldUpdate = (m_attackPathUpdateTimer >= UPDATE_INTERVAL) && (moved > 8.0f);

    if (shouldUpdate)
    {
        m_attackPathUpdateTimer = 0.0f;
        float k = 0.25f;
        m_attackStartFrozen = m_attackStartFrozen + (startPos - m_attackStartFrozen) * k;
    }

    Math::Vec2 start = m_attackStartFrozen;
    Math::Vec2 end = m_attackEndLive;

    float dx = end.x - start.x;
    float dy = end.y - start.y;

    float stepX = std::clamp(std::abs(dx) * 0.35f, 18.0f, 80.0f);
    float signX = (dx >= 0.f) ? 1.f : -1.f;

    float midY = start.y + std::clamp(dy * 0.5f, -60.f, 60.f);

    m_attackC1 = { start.x + signX * stepX, start.y };
    m_attackC2 = { m_attackC1.x, midY };
    m_attackC3 = { end.x, midY };

    // Precompute total length for travel sync
    auto segLen = [&](Math::Vec2 a, Math::Vec2 b) { return (b - a).Length(); };
    float L0 = segLen(start, m_attackC1);
    float L1 = segLen(m_attackC1, m_attackC2);
    float L2 = segLen(m_attackC2, m_attackC3);
    float L3 = segLen(m_attackC3, end);
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
    bool is_interact_key_pressed, double dt)
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

    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;

    auto checkSource = [&](PulseSource& source) {
        if (!source.HasPulse()) return;
        if (Collision::CheckAABB(playerHitboxCenter, playerHitboxSize, source.GetPosition(), source.GetSize()))
        {
            float dist_sq = (playerHitboxCenter - source.GetPosition()).LengthSq();
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
            }
        }
        };

    for (auto& source : roomSources)
    {
        checkSource(source);
    }
    for (auto& source : hallwaySources)
    {
        checkSource(source);
    }

    for (auto& source : rooftopSources)
    {
        checkSource(source);
    }
    for (auto& source : undergroundSources)
    {
        checkSource(source);
    }

    bool is_near_charger = (closest_source != nullptr);
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

    const float TILE = 16.0f;                
    const float packetLen = 24.0f;
    const float packetGap = 12.0f;
    const float packetLen2Mul = 0.75f;
    const float packetThick2Mul = 0.85f;

    auto snapTile = [&](Math::Vec2 p) -> Math::Vec2
        {
            float gx = std::round(p.x / TILE) * TILE;
            float gy = std::round(p.y / TILE) * TILE;
            return { gx, gy };
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

    auto axisDir = [&](Math::Vec2 d) -> Math::Vec2
        {
            if (std::abs(d.x) >= std::abs(d.y))
                return Math::Vec2{ (d.x >= 0.f) ? 1.f : -1.f, 0.f };
            else
                return Math::Vec2{ 0.f, (d.y >= 0.f) ? 1.f : -1.f };
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
        drawSegment_Tiled(p2, p3);
        drawSegment_Tiled(p3, p4);

        drawCornerAuto(p0, p1, p2);
        drawCornerAuto(p1, p2, p3);
        drawCornerAuto(p2, p3, p4);

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

                    drawPacketRect(headS, packetLen, 6.0f);

                    if (packetGap > 0.0f)
                    {
                        drawPacketRect(headS - packetGap,
                            packetLen * packetLen2Mul,
                            6.0f * packetThick2Mul);
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
