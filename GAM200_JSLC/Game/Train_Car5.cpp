// Train_Car5.cpp - FourthTrain (Car5): valve interaction, water VFX, encounter script

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
#include <cstdint>

#ifdef __EMSCRIPTEN__
static std::string PatchShaderForWebGL330(const std::string& src)
{
    std::string out = src;
    const std::string from = "#version 330 core";
    auto pos = out.find(from);
    if (pos != std::string::npos)
    {
        size_t endPos = pos + from.size();
        if (endPos < out.size() && out[endPos] == '\n')
            ++endPos;
        out.erase(pos, endPos - pos);
        out = "#version 300 es\nprecision highp float;\n" + out;
    }
    return out;
}
#endif

// GLSL 셰이더를 컴파일하고 핸들을 반환함 (실패 시 0 반환함, WebGL 빌드에서는 자동 패치함)
static GLuint CompileGLShader(GLenum type, const char* src)
{
#ifdef __EMSCRIPTEN__
    std::string patched = PatchShaderForWebGL330(std::string(src));
    const char* code = patched.c_str();
#else
    const char* code = src;
#endif

    GLuint shader = GL::CreateShader(type);
    GL::ShaderSource(shader, 1, &code, nullptr);
    GL::CompileShader(shader);

    GLint ok = 0;
    GL::GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        GL::GetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log)), nullptr, log);
        Logger::Instance().Log(Logger::Severity::Error, "Train valve water shader compile failed:\n%s", log);
        GL::DeleteShader(shader);
        return 0;
    }//
    return shader;
}

// 컴파일된 VS·FS를 링크해 셰이더 프로그램 핸들을 반환함 (실패 시 0 반환함)
static GLuint LinkGLProgram(GLuint vs, GLuint fs)
{
    GLuint program = GL::CreateProgram();
    GL::AttachShader(program, vs);
    GL::AttachShader(program, fs);
    GL::LinkProgram(program);

    GLint ok = 0;
    GL::GetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        GL::GetProgramInfoLog(program, static_cast<GLsizei>(sizeof(log)), nullptr, log);
        Logger::Instance().Log(Logger::Severity::Error, "Train valve water program link failed:\n%s", log);
        GL::DeleteProgram(program);
        return 0;
    }
    return program;
}

namespace {
// GPU-instanced valve water shader (GLSL 330 core — patched for WebGL2 in __EMSCRIPTEN__ builds)
constexpr const char* kValveWaterVS = R"GLSL(
#version 330 core
layout (location = 0) in vec2 aPos;

layout (location = 1) in vec2 iCenter;
layout (location = 2) in vec2 iHalfSize;
layout (location = 3) in float iAlpha;
layout (location = 4) in float iLayer;

uniform mat4 projection;

out vec4 vColor;

void main()
{
    vec2 corner = aPos * 2.0; // (-0.5..0.5) -> (-1..1)
    vec2 world = iCenter + corner * iHalfSize;

    // Tiny screen-space offsets from old multi-quad look (layer encoded as 0/1/2)
    if (iLayer > 1.5)
        world += vec2(-1.8, -1.0);
    else if (iLayer > 0.5)
        world += vec2(1.5, 1.5);

    vec3 rgb = vec3(0.16, 0.74, 0.98);
    float a = iAlpha;
    if (iLayer > 1.5)
    {
        rgb = vec3(0.03, 0.25, 0.55);
        a *= 0.28;
    }
    else if (iLayer > 0.5)
    {
        rgb = vec3(0.72, 0.95, 1.00);
        a *= 0.62;
    }
    else
    {
        rgb = vec3(0.16, 0.74, 0.98);
        a *= 0.55;
    }

    vColor = vec4(rgb, a);
    gl_Position = projection * vec4(world, 0.0, 1.0);
}
)GLSL";

constexpr const char* kValveWaterFS = R"GLSL(
#version 330 core
in vec4 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vColor;
}
)GLSL";
} // namespace

// 플레이어가 근접하고 마우스가 밸브 영역 위에 있을 때 밸브 상호작용 커서를 허용함
bool Train::IsValveMouseHoverable(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorldPos) const
{
    const Math::Vec2 valveWorld = { MIN_X + m_trainOffset + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
    // Interaction range widened so player can rotate from noticeably farther away.
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, valveWorld, { 560.0f, 320.0f }))
        return false;

    return Collision::CheckPointInAABB(mouseWorldPos, valveWorld, m_valveVisualSize)
        || Collision::CheckAABB(mouseWorldPos, { 32.0f, 32.0f }, valveWorld, m_valveVisualSize);
}

// 지정 월드 좌표·커서 히트박스가 열차 충돌 히트박스 위에 있는지 반환함
bool Train::IsPointOverWorldCollisionAABB(Math::Vec2 worldPos, Math::Vec2 cursorHitboxSize) const
{
    auto test = [&](const Math::Vec2& wc, const Math::Vec2& sz) {
        return Collision::CheckPointInAABB(worldPos, wc, sz)
            || Collision::CheckAABB(worldPos, cursorHitboxSize, wc, sz);
    };
    const float tl = MIN_X + m_trainOffset;
    for (const auto& hb : m_trainHitboxes)
    {
        if (!hb.collision)
            continue;
        const Math::Vec2 wc = { tl + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        if (test(wc, hb.size))
            return true;
    }
    for (const auto& hb : m_staticWorldHitboxes)
    {
        if (!hb.collision)
            continue;
        const Math::Vec2 wc = { MIN_X + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        if (test(wc, hb.size))
            return true;
    }
    return false;
}

// 밸브 물 파티클 위치·속도·수명을 갱신하고 바닥 충돌 시 튀는 파티클을 추가 생성함
void Train::UpdateValveWaterParticles(float dt)
{
    const float gravityY = -1200.0f;
    const float floorY = MIN_Y - 80.0f;
    const Math::Vec2 valveWorld = { MIN_X + m_trainOffset + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
    m_valveSplashScratch.clear();

    // 1) update existing particles
    for (auto& p : m_valveWaterParticles)
    {
        const float prevY = p.pos.y;
        p.vel.y += gravityY * dt;
        p.pos += p.vel * dt;
        p.life -= dt;

        // Floor collision -> burst splash particles
        if (p.life > 0.0f && p.vel.y < -120.0f && p.maxLife > 0.45f && prevY >= floorY && p.pos.y < floorY)
        {
            const int splashCount = 3 + static_cast<int>(m_valvePressureT * 6.0f);
            for (int i = 0; i < splashCount; ++i)
            {
                const float fi = static_cast<float>(i);
                const float ph = m_valveWaterAnimTime * 6.5f + fi * 1.37f + static_cast<float>(m_valveParticleCounter % 29u);
                ValveWaterParticle sp{};
                sp.pos = { p.pos.x + std::sin(ph) * 10.0f, floorY + 2.0f };
                sp.vel = { std::sin(ph * 1.9f) * (70.0f + m_valvePressureT * 120.0f),
                           140.0f + m_valvePressureT * 230.0f + std::cos(ph) * 40.0f };
                const float s = 7.0f + m_valvePressureT * 8.0f;
                sp.size = { s, s * 0.9f };
                sp.maxLife = 0.22f + m_valvePressureT * 0.22f;
                sp.life = sp.maxLife;
                sp.alpha = 0.30f + m_valvePressureT * 0.35f;
                m_valveSplashScratch.push_back(sp);
            }
            p.life = 0.0f;
        }
    }
    m_valveWaterParticles.erase(
        std::remove_if(m_valveWaterParticles.begin(), m_valveWaterParticles.end(),
            [floorY](const ValveWaterParticle& p) { return p.life <= 0.0f || p.pos.y < floorY; }),
        m_valveWaterParticles.end());
    if (!m_valveSplashScratch.empty())
        m_valveWaterParticles.insert(m_valveWaterParticles.end(), m_valveSplashScratch.begin(),
                                     m_valveSplashScratch.end());

    // ApplyValveWaterDamageToEnemies는 Train::Update 끝에서 호출 — 스크립트가 SetPosition으로
    // 드론/로봇을 배치한 뒤에 넉백·데미지가 적용되도록 한다.

    // 2) spawn based on pressure (swampy-like: starts weak, ramps up)
    if (m_valvePressureT <= 0.01f)
    {
        m_valveParticleSpawnCarry = 0.0f;
        return;
    }

    const float spawnRateLeft = 52.0f + m_valvePressureT * 290.0f;
    const float spawnRateRight = 92.0f + m_valvePressureT * 420.0f;
    const float spawnAvg = 0.5f * (spawnRateLeft + spawnRateRight);
    m_valveParticleSpawnCarry += (spawnAvg * 2.0f) * dt;

    while (m_valveParticleSpawnCarry >= 1.0f)
    {
        m_valveParticleSpawnCarry -= 1.0f;

        // Strong bias to the right nozzle (5:1) — primary anti-enemy jet.
        const float dirSign = ((m_valveParticleCounter % 6u) == 0u) ? -1.0f : 1.0f;
        const float ph = static_cast<float>(m_valveParticleCounter) * 0.91f + m_valveWaterAnimTime * 2.4f;
        ++m_valveParticleCounter;

        ValveWaterParticle p{};
        const float outletJitterX = std::sin(ph * 1.3f) * 7.0f;
        const float outletJitterY = std::cos(ph * 1.9f) * 4.0f;
        float outletX = dirSign * 132.0f;
        if (dirSign > 0.0f)
            outletX += 42.0f; // right nozzle feels more "open"
        p.pos = { valveWorld.x + outletX + outletJitterX, valveWorld.y - 40.0f + outletJitterY };

        const float pressureSpeed = 80.0f + m_valvePressureT * 360.0f;
        float sideKick = dirSign * (180.0f + m_valvePressureT * 420.0f + std::sin(ph) * 64.0f);
        if (dirSign > 0.0f)
            sideKick *= 2.15f; // right stream: much stronger horizontal push

        float vertical = -pressureSpeed + std::cos(ph * 1.7f) * 32.0f;
        if (dirSign < 0.0f)
            vertical *= 0.50f; // slightly flatter than before
        if (dirSign > 0.0f)
            vertical *= 0.26f; // right stream: slightly lower launch angle
        p.vel = { sideKick, vertical };

        const float s = 18.0f + m_valvePressureT * 24.0f + std::sin(ph * 0.8f) * 3.4f;
        const float sMul = (dirSign > 0.0f) ? 1.22f : 1.12f;
        p.size = { s * 1.12f * sMul, s * 1.60f * sMul };
        p.maxLife = 0.55f + m_valvePressureT * 0.55f + std::abs(std::sin(ph * 0.6f)) * 0.18f;
        p.life = p.maxLife;
        p.alpha = 0.25f + m_valvePressureT * 0.55f;

        m_valveWaterParticles.push_back(p);
    }

    if (m_valveWaterParticles.size() > kMaxValveWaterParticles)
    {
        const std::size_t drop = m_valveWaterParticles.size() - kMaxValveWaterParticles;
        m_valveWaterParticles.erase(
            m_valveWaterParticles.begin(),
            m_valveWaterParticles.begin() + static_cast<std::ptrdiff_t>(drop));
    }
}


// 밸브 물 GPU 인스턴스 렌더링용 VAO·VBO·프로그램을 초기화함
void Train::InitValveWaterGpu()
{
    ShutdownValveWaterGpu();

    GLuint vs = CompileGLShader(GL_VERTEX_SHADER, kValveWaterVS);
    GLuint fs = CompileGLShader(GL_FRAGMENT_SHADER, kValveWaterFS);
    if (!vs || !fs)
        return;

    m_valveWaterProg = LinkGLProgram(vs, fs);
    GL::DeleteShader(vs);
    GL::DeleteShader(fs);
    if (!m_valveWaterProg)
        return;

    m_valveWaterLocProjection = GL::GetUniformLocation(m_valveWaterProg, "projection");

    float quadVerts[] = {
        -0.5f,  0.5f,
         0.5f, -0.5f,
        -0.5f, -0.5f,
        -0.5f,  0.5f,
         0.5f,  0.5f,
         0.5f, -0.5f
    };

    GL::GenVertexArrays(1, &m_valveWaterVAO);
    GL::GenBuffers(1, &m_valveWaterQuadVBO);
    GL::GenBuffers(1, &m_valveWaterInstVBO);

    GL::BindVertexArray(m_valveWaterVAO);

    GL::BindBuffer(GL_ARRAY_BUFFER, m_valveWaterQuadVBO);
    GL::BufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    GL::VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    GL::EnableVertexAttribArray(0);

    constexpr GLsizei instStride = sizeof(ValveWaterGpuInstance);
    GL::BindBuffer(GL_ARRAY_BUFFER, m_valveWaterInstVBO);
    m_valveWaterInstPoolBytes = sizeof(ValveWaterGpuInstance) * 4096u;
    GL::BufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_valveWaterInstPoolBytes), nullptr, GL_DYNAMIC_DRAW);

    GL::VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, instStride, (void*)offsetof(ValveWaterGpuInstance, center));
    GL::EnableVertexAttribArray(1);
    GL::VertexAttribDivisor(1, 1);

    GL::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, instStride, (void*)offsetof(ValveWaterGpuInstance, halfSize));
    GL::EnableVertexAttribArray(2);
    GL::VertexAttribDivisor(2, 1);

    GL::VertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, instStride, (void*)offsetof(ValveWaterGpuInstance, alpha));
    GL::EnableVertexAttribArray(3);
    GL::VertexAttribDivisor(3, 1);

    GL::VertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, instStride, (void*)offsetof(ValveWaterGpuInstance, layer));
    GL::EnableVertexAttribArray(4);
    GL::VertexAttribDivisor(4, 1);

    GL::BindVertexArray(0);
    GL::BindBuffer(GL_ARRAY_BUFFER, 0);

    m_valveWaterGpuReady = true;
}

// 밸브 물 GPU 리소스(VAO·VBO·셰이더 프로그램)를 해제함
void Train::ShutdownValveWaterGpu()
{
    if (m_valveWaterVAO) { GL::DeleteVertexArrays(1, &m_valveWaterVAO); m_valveWaterVAO = 0; }
    if (m_valveWaterQuadVBO) { GL::DeleteBuffers(1, &m_valveWaterQuadVBO); m_valveWaterQuadVBO = 0; }
    if (m_valveWaterInstVBO) { GL::DeleteBuffers(1, &m_valveWaterInstVBO); m_valveWaterInstVBO = 0; }
    if (m_valveWaterProg) { GL::DeleteProgram(m_valveWaterProg); m_valveWaterProg = 0; }
    m_valveWaterLocProjection = -1;
    m_valveWaterInstPoolBytes = 0;
    m_valveWaterGpuReady = false;
}

// 파티클 인스턴스 데이터를 GPU에 업로드하고 GPU 인스턴스 드로 콜로 물을 렌더링함
void Train::UploadAndDrawValveWaterGpu(const Math::Matrix& projection, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_valveWaterGpuReady || !m_valveWaterProg || !m_valveWaterVAO)
        return;

    const float valveY = MIN_Y + m_valveLocalCenter.y;

    const float halfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float visL = cameraPos.x - halfW - 900.0f;
    const float visR = cameraPos.x + halfW + 900.0f;

    m_valveWaterGpuScratch.clear();
    m_valveWaterGpuScratch.reserve(m_valveWaterParticles.size() * 3u + 64u);

    for (const auto& p : m_valveWaterParticles)
    {
        if (p.pos.x < visL || p.pos.x > visR)
            continue;

        const float lifeT = (p.maxLife > 0.0f) ? std::clamp(p.life / p.maxLife, 0.0f, 1.0f) : 0.0f;
        const float alphaBase = p.alpha * (0.35f + lifeT * 0.65f);
        const float stretch = 1.0f + std::min(1.8f, std::abs(p.vel.y) / 320.0f);
        const float depthT = std::clamp((valveY - p.pos.y) / 360.0f, 0.0f, 1.0f);

        const float widthMul = 1.65f + depthT * 2.45f;
        const float rightOnlyBoost = (p.vel.x > 0.0f) ? 1.48f : 1.0f;
        const Math::Vec2 mainSize = { p.size.x * widthMul * rightOnlyBoost, p.size.y * stretch * 1.20f };

        const Math::Vec2 half = { mainSize.x * 0.5f, mainSize.y * 0.5f };

        ValveWaterGpuInstance body{};
        body.center = p.pos;
        body.halfSize = half;
        body.alpha = alphaBase;
        body.layer = 0.0f;
        m_valveWaterGpuScratch.push_back(body);

        ValveWaterGpuInstance core{};
        core.center = p.pos + Math::Vec2{ 1.5f, 1.5f };
        core.halfSize = { mainSize.x * 0.56f * 0.5f, mainSize.y * 0.76f * 0.5f };
        core.alpha = alphaBase;
        core.layer = 1.0f;
        m_valveWaterGpuScratch.push_back(core);

        ValveWaterGpuInstance sh{};
        sh.center = p.pos + Math::Vec2{ -1.8f, -1.0f };
        sh.halfSize = { mainSize.x * 0.66f * 0.5f, mainSize.y * 0.74f * 0.5f };
        sh.alpha = alphaBase;
        sh.layer = 2.0f;
        m_valveWaterGpuScratch.push_back(sh);
    }

    if (m_valveWaterGpuScratch.empty())
        return;

    const GLsizeiptr bytesNeeded =
        static_cast<GLsizeiptr>(m_valveWaterGpuScratch.size() * sizeof(ValveWaterGpuInstance));

    GL::BindBuffer(GL_ARRAY_BUFFER, m_valveWaterInstVBO);
    if (bytesNeeded > static_cast<GLsizeiptr>(m_valveWaterInstPoolBytes))
    {
        const GLsizeiptr newBytes =
            (bytesNeeded * 3) / 2 + static_cast<GLsizeiptr>(sizeof(ValveWaterGpuInstance)) * 256;
        GL::BufferData(GL_ARRAY_BUFFER, newBytes, nullptr, GL_DYNAMIC_DRAW);
        m_valveWaterInstPoolBytes = static_cast<std::uint32_t>(newBytes);
    }

    GL::BufferSubData(GL_ARRAY_BUFFER, 0,
                      static_cast<GLsizeiptr>(m_valveWaterGpuScratch.size() * sizeof(ValveWaterGpuInstance)),
                      m_valveWaterGpuScratch.data());
    GL::BindBuffer(GL_ARRAY_BUFFER, 0);

    GL::UseProgram(m_valveWaterProg);
    GL::UniformMatrix4fv(m_valveWaterLocProjection, 1, GL_FALSE, projection.Ptr());

    GL::BindVertexArray(m_valveWaterVAO);
    GL::DrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(m_valveWaterGpuScratch.size()));
    GL::BindVertexArray(0);
    GL::UseProgram(0);
}

// 밸브 분사 범위 내 드론·로봇에게 매 프레임 물 밀어내기와 데미지를 적용함
void Train::ApplyValveWaterDamageToEnemies(float dt)
{
    if (dt <= 0.0f)
        return;
    if (m_valvePressureT <= 0.01f && m_valveWaterParticles.empty())
        return;

    const float fdt = static_cast<float>(dt);

    const float valveY = MIN_Y + m_valveLocalCenter.y;
    const float tl       = MIN_X + m_trainOffset;
    const float valveX   = tl + m_valveLocalCenter.x;
    const float trainRight = tl + m_totalTrainWidth;

    auto hitsWater = [&](const Math::Vec2& enemyCenter, const Math::Vec2& enemySize, float extraEnemyPadding) -> bool
    {
        if (m_valveWaterParticles.empty())
            return false;
        for (const auto& p : m_valveWaterParticles)
        {
            if (p.life <= 0.0f)
                continue;

            const float lifeT = (p.maxLife > 0.0f) ? std::clamp(p.life / p.maxLife, 0.0f, 1.0f) : 0.0f;
            const float stretch = 1.0f + std::min(1.8f, std::abs(p.vel.y) / 320.0f);
            const float depthT = std::clamp((valveY - p.pos.y) / 360.0f, 0.0f, 1.0f);
            const float widthMul = 1.65f + depthT * 2.45f;

            const bool isRightJet = (p.vel.x > 0.0f);
            const float jetBoost = isRightJet ? 1.48f : 1.0f;
            const float hitBoost = isRightJet ? 1.35f : 1.0f;

            Math::Vec2 paddedEnemySize = enemySize;
            paddedEnemySize.x += extraEnemyPadding;
            paddedEnemySize.y += extraEnemyPadding;

            const Math::Vec2 mainSize = { p.size.x * widthMul * jetBoost * hitBoost, p.size.y * stretch * 1.20f };
            if (!Collision::CheckAABB(p.pos, mainSize, enemyCenter, paddedEnemySize))
                continue;

            if (m_valvePressureT < 0.05f && lifeT < 0.05f)
                continue;

            return true;
        }
        return false;
    };

    // 밸브가 열린 동안: 탱크 우측 저공 구역을 물줄기로 간주 (파티클이 얇을 때도 피격 판정 보강)
    auto inSprayBand = [&](const Math::Vec2& enemyCenter, const Math::Vec2& enemySize) -> bool
    {
        if (m_valvePressureT < 0.05f)
            return false;
        const float xL = valveX - 220.f;
        const float xR = trainRight + 120.f;
        const float feet = enemyCenter.y - enemySize.y * 0.5f;
        const float head = enemyCenter.y + enemySize.y * 0.5f;
        if (enemyCenter.x < xL || enemyCenter.x > xR)
            return false;
        // 물탱크 발판·저공 드론 높이대
        if (feet > MIN_Y + 420.f || head < MIN_Y + 40.f)
            return false;
        return true;
    };

    if (m_droneManager)
    {
        auto& drones = m_droneManager->GetDrones();
        for (size_t i = 0; i < drones.size(); ++i)
        {
            auto& d = drones[i];
            if (d.IsDead())
                continue;

            if (i < m_droneWaterCd.size() && m_droneWaterCd[i] > 0.0f)
                m_droneWaterCd[i] -= dt;

            if (i < m_droneWaterCd.size() && m_droneWaterCd[i] > 0.0f)
                continue;

            const bool wet = hitsWater(d.GetPosition(), d.GetSize(), 36.0f)
                             || inSprayBand(d.GetPosition(), d.GetSize());
            if (!wet)
                continue;

            // Train 맵은 Drone::Update를 돌리지 않아 ApplyKnockback 슬라이드가 보이지 않음 → 위치로 즉시 넉백
            Math::Vec2 wp = d.GetPosition();
            wp.x += 280.f;
            d.SetPosition(wp);
            d.SetVelocity({ 0.f, 0.f });
            const float dmg = d.GetMaxHP() * 0.80f;
            d.TakeDamage(dmg);
            if (i < m_droneWaterCd.size())
                m_droneWaterCd[i] = 1.15f;
        }
    }

    if (m_carTransportDroneManager)
    {
        auto& ctd = m_carTransportDroneManager->GetDrones();
        for (size_t i = 0; i < ctd.size(); ++i)
        {
            auto& d = ctd[i];
            if (d.IsDead())
                continue;

            if (i < m_carTransportDroneWaterCd.size() && m_carTransportDroneWaterCd[i] > 0.0f)
                m_carTransportDroneWaterCd[i] -= dt;

            if (i < m_carTransportDroneWaterCd.size() && m_carTransportDroneWaterCd[i] > 0.0f)
                continue;

            const bool wet = hitsWater(d.GetPosition(), d.GetSize(), 36.0f)
                             || inSprayBand(d.GetPosition(), d.GetSize());
            if (!wet)
                continue;

            Math::Vec2 wp = d.GetPosition();
            wp.x += 280.f;
            d.SetPosition(wp);
            d.SetCarTransportPersistHover(false);
            d.SetCarTransportHover(false);
            d.SetVelocity({ 0.f, 0.f });
            const float dmg = d.GetMaxHP() * 0.80f;
            d.TakeDamage(dmg);
            if (i < m_carTransportDroneWaterCd.size())
                m_carTransportDroneWaterCd[i] = 1.15f;
        }
    }

    for (size_t i = 0; i < m_robots.size(); ++i)
    {
        auto& r = m_robots[i];
        if (r.IsDead())
            continue;

        const bool wet = hitsWater(r.GetPosition(), r.GetSize(), 28.0f)
                         || inSprayBand(r.GetPosition(), r.GetSize());
        if (!wet)
            continue;

        Math::Vec2 p = r.GetPosition();
        p.x += 240.f * fdt;
        r.SetPosition(p);
        r.TakeDamage(r.GetMaxHP() * 0.5f * fdt, false);
    }
}



// Car5 조우 스크립트를 매 프레임 실행함 — 드론·로봇의 대형 이동과 물 회피 행동을 관리함
void Train::UpdateTrainEncounterScript(float dt, Player& player)
{
    if (!m_car5EncounterActive)
        return;

    const float                 tl        = MIN_X + m_trainOffset;
    const float                 trainRight = tl + m_totalTrainWidth;
    const bool                  fleeWater = (m_valvePressureT > 0.10f);
    const Math::Vec2          ppos      = player.GetHitboxCenter();
    const bool                hidePlayer =
        IsPlayerHiding(ppos, player.GetHitboxSize(), player.IsCrouching());
    constexpr float kCar5DroneApproach = 268.f;

    if (m_droneManager)
    {
        auto& drones = m_droneManager->GetDrones();
        const float c5L = tl + m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float c5R = c5L + m_car5Width;
        const float formationCenter =
            hidePlayer ? (c5L + c5R) * 0.5f
                               + std::sin(m_encounterScriptTime * 3.05f) * (c5R - c5L) * 0.30f
                       : std::clamp(ppos.x, c5L + 260.f, c5R - 260.f);
        const float formationSpacingBase = 228.f;
        const float car5HoverY =
            m_car3SirenHbValid ? (MIN_Y + m_car3SirenHb.localCenter.y + 55.f)
                               : (Train::MIN_Y + kTrainFlatbedDeckTopLocalY + 380.f);
        int car5LiveIdx = 0;
        for (size_t i = 0; i < drones.size(); ++i)
        {
            auto& d = drones[i];
            if (d.IsDead())
                continue;
            if (d.IsHit() || d.IsStunned())
                continue;
            if (d.GetTrainCarSegment() != 5)
                continue;

            Math::Vec2 pos = d.GetPosition();
            const float bobY =
                std::sin(m_encounterScriptTime * 2.45f + static_cast<float>(i) * 0.73f) * 5.f;
            const float hoverY = car5HoverY + bobY;

            float dx = 0.f;
            if (fleeWater)
            {
                // 물 회피: 모두 같은 속도로 오른쪽(열차 끝)으로만 이동
                dx = 415.f * dt;
                const float capR = trainRight - 95.f;
                if (pos.x + dx > capR)
                    dx = std::max(0.f, capR - pos.x);
            }
            else
            {
                const float spacing = formationSpacingBase + static_cast<float>(car5LiveIdx % 6) * 31.f;
                const float slot    = static_cast<float>(car5LiveIdx) - 2.5f;
                const float desiredX =
                    std::clamp(formationCenter + slot * spacing, c5L + 110.f, c5R - 110.f);
                const float ap = kCar5DroneApproach + static_cast<float>(car5LiveIdx % 5) * 19.f;
                dx             = std::clamp(desiredX - pos.x, -ap * dt, ap * dt);
            }

            pos.x += dx;
            pos.y = hoverY;
            d.SetPosition(pos);
            d.SetVelocity({ 0.f, 0.f });
            d.SetBaseSpeed(72.f + static_cast<float>(car5LiveIdx % 7) * 14.f);
            ++car5LiveIdx;
        }
    }

    UpdateTrainRobotsAI(dt, player);
}


// ---------------------------------------------------------------------------
// 밸브 물 파티클 VFX를 GPU 인스턴싱으로 렌더링함 (파티클 없고 압력 없으면 건너뜀)
// ---------------------------------------------------------------------------

void Train::DrawValveWaterVFX(Shader& colorShader, const Math::Matrix& worldProjection, Math::Vec2 cameraPos,
                              float viewHalfW) const
{
    (void)colorShader;

    if (m_valveWaterParticles.empty() && m_valvePressureT <= 0.01f)
        return;

    UploadAndDrawValveWaterGpu(worldProjection, cameraPos, viewHalfW);
}


// ---------------------------------------------------------------------------
// Draw – draws rail tiles and train car images
// ---------------------------------------------------------------------------