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
    const char* circuitPath = "Asset/PulseCircuit.png";
    const char* fluidPath = "Asset/PulseFluid.png";

    auto loadTexture = [&](const char* path, GLuint& outTexID)
        {
            outTexID = 0;

            GL::GenTextures(1, &outTexID);
            GL::BindTexture(GL_TEXTURE_2D, outTexID);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GL::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            int width = 0, height = 0, nrChannels = 0;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

            if (data)
            {
                GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
                GL::TexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                GL::GenerateMipmap(GL_TEXTURE_2D);
                stbi_image_free(data);

                Logger::Instance().Log(Logger::Severity::Info, "%s loaded for VFX.", path);
            }
            else
            {
                Logger::Instance().Log(Logger::Severity::Error, "Failed to load %s!", path);
                stbi_image_free(data);
                outTexID = 0;
            }
        };

    loadTexture(circuitPath, m_circuitTexID);
    loadTexture(fluidPath, m_fluidTexID);

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

    GL::DeleteVertexArrays(1, &m_pulseVAO);
    GL::DeleteBuffers(1, &m_pulseVBO);

    m_circuitTexID = 0;
    m_fluidTexID = 0;
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

    // Smoothly follow player's start (reduces jitter)
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

    PulseSource* closest_source = nullptr;
    float closest_dist_sq = -1.0f;
    bool mouseOnSource = false;

    auto checkSource = [&](PulseSource& source) {
        if (!source.HasPulse()) return;
        if (Collision::CheckAABB(playerHitboxCenter, playerHitboxSize, source.GetPosition(), source.GetSize()))
        {
            float dist_sq = (playerHitboxCenter - source.GetPosition()).LengthSq();
            if (closest_source == nullptr || dist_sq < closest_dist_sq)
            {
                closest_source = &source;
                closest_dist_sq = dist_sq;
                // Check if mouse is clicking on this pulse source
                mouseOnSource = Collision::CheckPointInAABB(mouseWorldPos, source.GetPosition(), source.GetSize());
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

    bool is_near_charger = (closest_source != nullptr && mouseOnSource);
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

    const float circuitThickness = 6.0f;             
    const float fluidThickness = circuitThickness; 
    const float chargeThickness = circuitThickness; 

    const float nodeRadius = circuitThickness * 0.33f; 

    const float packetLen = 24.0f;         
    const float packetGap = 12.0f;         
    const float packetLen2Mul = 0.75f;       
    const float packetThick2Mul = 0.85f;      

    auto drawSprite = [&](Math::Vec2 p, Math::Vec2 size)
        {
            Math::Matrix model =
                Math::Matrix::CreateTranslation(p) *
                Math::Matrix::CreateScale(size);
            shader.setMat4("model", model);
            GL::DrawArrays(GL_TRIANGLES, 0, 6);
        };

    auto drawNode = [&](Math::Vec2 p, float r)
        {
            drawSprite(p, { r, r });
            drawSprite(p + Math::Vec2{ r, 0.f }, { r * 0.7f, r * 0.7f });
            drawSprite(p + Math::Vec2{ -r, 0.f }, { r * 0.7f, r * 0.7f });
            drawSprite(p + Math::Vec2{ 0.f,  r }, { r * 0.7f, r * 0.7f });
            drawSprite(p + Math::Vec2{ 0.f, -r }, { r * 0.7f, r * 0.7f });
        };

    auto drawManhattanSegment = [&](Math::Vec2 a, Math::Vec2 b, float thickness)
        {
            Math::Vec2 d = b - a;
            float ax = std::abs(d.x);
            float ay = std::abs(d.y);
            if (ax < 0.001f && ay < 0.001f) return;

            Math::Vec2 center = (a + b) * 0.5f;
            if (ax >= ay) drawSprite(center, { ax, thickness });
            else          drawSprite(center, { thickness, ay });
        };

    auto safeDir = [&](Math::Vec2 a, Math::Vec2 b) -> Math::Vec2
        {
            Math::Vec2 v = b - a;
            float l = v.Length();
            if (l < 0.0001f) return Math::Vec2{ 1.f, 0.f };
            return v / l;
        };

    if (m_isAttacking && m_attackPathValid)
    {
        Math::Vec2 start = m_attackStartFrozen;
        Math::Vec2 c1 = m_attackC1;
        Math::Vec2 c2 = m_attackC2;
        Math::Vec2 c3 = m_attackC3;
        Math::Vec2 end = m_attackEndLive;

        auto segLen = [&](Math::Vec2 a, Math::Vec2 b) { return (b - a).Length(); };
        float L0 = segLen(start, c1);
        float L1 = segLen(c1, c2);
        float L2 = segLen(c2, c3);
        float L3 = segLen(c3, end);
        float totalLen = L0 + L1 + L2 + L3;

        GL::BindTexture(GL_TEXTURE_2D, m_circuitTexID);

        drawManhattanSegment(start, c1, circuitThickness);
        drawManhattanSegment(c1, c2, circuitThickness);
        drawManhattanSegment(c2, c3, circuitThickness);
        drawManhattanSegment(c3, end, circuitThickness);

        drawNode(start, nodeRadius);
        drawNode(c1, nodeRadius * 0.90f);
        drawNode(c2, nodeRadius * 0.90f);
        drawNode(c3, nodeRadius * 0.90f);
        drawNode(end, nodeRadius * 1.05f);

        if (m_attackPacketActive && totalLen >= 1.0f)
        {
            auto pointOnPath = [&](float s) -> Math::Vec2
                {
                    if (s <= L0) return start + safeDir(start, c1) * s; s -= L0;
                    if (s <= L1) return c1 + safeDir(c1, c2) * s; s -= L1;
                    if (s <= L2) return c2 + safeDir(c2, c3) * s; s -= L2;
                    if (s <= L3) return c3 + safeDir(c3, end) * s;
                    return end;
                };

            auto dirOnPath = [&](float s) -> Math::Vec2
                {
                    if (s <= L0) return safeDir(start, c1); s -= L0;
                    if (s <= L1) return safeDir(c1, c2);    s -= L1;
                    if (s <= L2) return safeDir(c2, c3);    s -= L2;
                    return safeDir(c3, end);
                };

            float travelTime = std::max(m_attackTravelTime, 0.001f);
            float phase = std::clamp(m_attackElapsed / travelTime, 0.0f, 1.0f);

            if (phase < 1.0f)
            {
                float headS = phase * totalLen;

                GL::BindTexture(GL_TEXTURE_2D, m_fluidTexID);

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

                        if (ax >= ay) drawSprite(center, { visualLen, thickness });
                        else          drawSprite(center, { thickness, visualLen });
                    };

                drawPacketRect(headS, packetLen, fluidThickness);

                if (packetGap > 0.0f)
                {
                    drawPacketRect(headS - packetGap,
                        packetLen * packetLen2Mul,
                        fluidThickness * packetThick2Mul);
                }
            }
        }

        GL::BindVertexArray(0);
        GL::Disable(GL_BLEND);
        return;
    }

    if (m_isCharging)
    {
        GL::BindTexture(GL_TEXTURE_2D, m_circuitTexID);

        Math::Vec2 start = m_chargeStartPos;
        Math::Vec2 end = m_chargeEndPos;

        Math::Vec2 diff = end - start;
        if (diff.Length() >= 1.0f)
        {
            Math::Vec2 cornerA = { end.x, start.y };
            Math::Vec2 cornerB = { start.x, end.y };

            float lenA = (cornerA - start).Length() + (end - cornerA).Length();
            float lenB = (cornerB - start).Length() + (end - cornerB).Length();
            Math::Vec2 corner = (lenA <= lenB) ? cornerA : cornerB;

            drawManhattanSegment(start, corner, chargeThickness);
            drawManhattanSegment(corner, end, chargeThickness);

            drawNode(start, nodeRadius * 0.85f);
            drawNode(corner, nodeRadius * 0.80f);
            drawNode(end, nodeRadius * 0.95f);
        }

        GL::BindVertexArray(0);
        GL::Disable(GL_BLEND);
        return;
    }

    GL::BindVertexArray(0);
    GL::Disable(GL_BLEND);
}