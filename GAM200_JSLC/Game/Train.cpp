// Train.cpp

#include "Train.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "DroneManager.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/Logger.hpp"
#include "../OpenGL/GLWrapper.hpp"
#include "MapObjectConfig.hpp"
#include "Robot.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>


// ---------------------------------------------------------------------------
// Helper – pixel-space hitbox definition (image coords, Y from top of image)
//   Assumes each car image renders at its pixel size in world units.
//   localCenter.x = from train's initial left edge (MIN_X when offset=0)
//   localCenter.y = from map bottom (MIN_Y), Y going UP in world space
// ---------------------------------------------------------------------------
static constexpr float ASSUMED_IMG_HEIGHT = 1080.0f; // expected car image height in pixels

static float WrapAnglePi(float a)
{
    while (a > 3.14159265359f) a -= 6.28318530718f;
    while (a < -3.14159265359f) a += 6.28318530718f;
    return a;
}

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
    }
    return shader;
}

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

// Convert pixel (px,py) top-left + size to world local center (Y-flipped)
static Train::TrainHitbox MakeHitbox(float carOffset, float px, float py, float pw, float ph,
                                     bool collision = true,
                                     Train::TrainHitboxKind kind = Train::TrainHitboxKind::Solid)
{
    float cx = carOffset + px + pw * 0.5f;
    float cy = ASSUMED_IMG_HEIGHT - py - ph * 0.5f; // flip Y: pixel-from-top → world-from-bottom
    return { {cx, cy}, {pw, ph}, collision, kind };
}

// Third_Third 자동차 영역: 단일 바깥 박스 (표시용). collision=false면 플레이어 통과.
static void AppendCarSilhouette(std::vector<Train::TrainHitbox>& out, float carOffset,
                                float px, float py, float pw, float ph, bool collision)
{
    out.push_back(MakeHitbox(carOffset, px, py, pw, ph, collision, Train::TrainHitboxKind::Solid));
}

// Loose world-space band over the train cars for “riding” horizontal carry (jump / crouch).
static bool HitboxInTrainCarryBand(float trainWorldLeft, float totalTrainWidth,
                                   const Math::Vec2& hbCenter, const Math::Vec2& hbHalf)
{
    constexpr float margin = 56.0f;
    const float l = trainWorldLeft - margin;
    const float r = trainWorldLeft + totalTrainWidth + margin;
    if (hbCenter.x + hbHalf.x < l || hbCenter.x - hbHalf.x > r)
        return false;
    if (hbCenter.y + hbHalf.y < Train::MIN_Y + 24.0f)
        return false;
    if (hbCenter.y - hbHalf.y > Train::MIN_Y + Train::HEIGHT + 40.0f)
        return false;
    return true;
}


// ---------------------------------------------------------------------------
// Third_ThirdTrain — 자동차 운반 칸 6슬롯 (엔진 / 펄스 주입 / 직선 연쇄)
// ---------------------------------------------------------------------------
namespace {
constexpr float kCarInjectDurationSec   = 2.0f;
constexpr float kCarInjectPulseDrainSec = 22.0f;
constexpr float kCarInteractRangeSq   = 380.0f * 380.0f;
constexpr float kCarSkillAnchorRangeSq = 400.0f * 400.0f;
constexpr float kChainDetonateRadius  = 440.0f;
constexpr float kChainDetonateStun    = 2.0f;
constexpr float kMoorePulseShareRate  = 2.5f;
constexpr int   kCarTransportCount    = 6;

static const int kStraightCarPairs[][2] = {
    { 0, 1 }, { 1, 2 }, { 3, 4 }, { 4, 5 },
    { 0, 3 }, { 1, 4 }, { 2, 5 },
};
}

Math::Vec2 Train::CarTransportWorldCenter(int slotIndex) const
{
    const float trainLeft = MIN_X + m_trainOffset;
    const auto& s         = m_carTransportSlots[static_cast<size_t>(slotIndex)];
    return { trainLeft + s.localCenter.x, MIN_Y + s.localCenter.y };
}

bool Train::IsMooreAdjacentCarSlots(int a, int b)
{
    const int ra = a / 3, ca = a % 3;
    const int rb = b / 3, cb = b % 3;
    const int dr = std::abs(ra - rb), dc = std::abs(ca - cb);
    return dr <= 1 && dc <= 1 && (dr + dc) > 0;
}

int Train::FindCarTransportInjectTarget(Math::Vec2 playerHbCenter) const
{
    int   best   = -1;
    float bestD  = 1.e18f;
    for (int i = 0; i < kCarTransportCount; ++i)
    {
        if (m_carTransportSlots[static_cast<size_t>(i)].engineOn)
            continue;
        const Math::Vec2 w = CarTransportWorldCenter(i);
        const float      d = (playerHbCenter - w).LengthSq();
        if (d > kCarInteractRangeSq || d >= bestD)
            continue;
        bestD = d;
        best  = i;
    }
    return best;
}

void Train::ResetCarTransportSlotsToInitialState()
{
    const float c4 = m_car1Width + m_car2Width + m_car3Width;

    struct Def
    {
        float px, py, pw, ph;
        bool startEngine;
    };
    static const Def defs[kCarTransportCount] = {
        { 429.f, 129.f, 886.f, 304.f, true },   // 좌상 — 시동 ON
        { 1536.f, 129.f, 886.f, 304.f, false },
        { 2629.f, 129.f, 886.f, 304.f, false },
        { 432.f, 489.f, 876.f, 306.f, false },
        { 1539.f, 489.f, 876.f, 306.f, false },
        { 2631.f, 489.f, 876.f, 306.f, true }, // 우하 — 시동 ON
    };

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        Train::TrainHitbox hb = MakeHitbox(c4, defs[i].px, defs[i].py, defs[i].pw, defs[i].ph, false,
                                           TrainHitboxKind::Solid);
        m_carTransportSlots[static_cast<size_t>(i)].localCenter    = hb.localCenter;
        m_carTransportSlots[static_cast<size_t>(i)].halfSize       = hb.size * 0.5f;
        m_carTransportSlots[static_cast<size_t>(i)].engineOn       = defs[i].startEngine;
        m_carTransportSlots[static_cast<size_t>(i)].injectSeconds  = 0.f;
        m_carTransportSlots[static_cast<size_t>(i)].engineGlowTimer = 0.f;
    }
    m_carInjectFocusSlot = -1;
}

void Train::NotifyCarTransportInjectionInterrupted()
{
    for (auto& s : m_carTransportSlots)
        s.injectSeconds = 0.f;
    m_carInjectFocusSlot = -1;
}

bool Train::TryGetCarTransportSkillAnchor(Math::Vec2 playerHbCenter, Math::Vec2& outWorldAnchor) const
{
    int   best  = -1;
    float bestD = 1.e18f;
    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const Math::Vec2 w = CarTransportWorldCenter(i);
        const float      d = (playerHbCenter - w).LengthSq();
        if (d > kCarSkillAnchorRangeSq || d >= bestD)
            continue;
        bestD = d;
        best  = i;
    }
    if (best < 0)
        return false;
    outWorldAnchor = CarTransportWorldCenter(best);
    return true;
}

bool Train::TryGetCarTransportClickIgniteTarget(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                                Math::Vec2 mouseWorldPos, int& outSlotIndex) const
{
    const Math::Vec2 kCursorHitbox{ 32.f, 32.f };

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const auto& slot = m_carTransportSlots[static_cast<size_t>(i)];
        if (slot.engineOn)
            continue;

        const Math::Vec2 wc   = CarTransportWorldCenter(i);
        const Math::Vec2 full = slot.halfSize * 2.f;

        if (!Collision::CheckAABB(playerHbCenter, playerHbSize, wc, full))
            continue;

        const bool mouseOnCar = Collision::CheckPointInAABB(mouseWorldPos, wc, full)
                                || Collision::CheckAABB(mouseWorldPos, kCursorHitbox, wc, full);
        if (!mouseOnCar)
            continue;

        outSlotIndex = i;
        return true;
    }
    return false;
}

bool Train::TryIgniteCarTransportSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= kCarTransportCount)
        return false;

    auto& slot = m_carTransportSlots[static_cast<size_t>(slotIndex)];
    if (slot.engineOn)
        return false;

    slot.engineOn        = true;
    slot.injectSeconds   = 0.f;
    slot.engineGlowTimer = 1.35f;
    if (m_carInjectFocusSlot == slotIndex)
        m_carInjectFocusSlot = -1;

    FireStraightLineChainDetonations();

    Logger::Instance().Log(Logger::Severity::Info,
                           "Train: Car transport engine ON (click slot %d)", slotIndex);
    return true;
}

void Train::AppendCarTransportStraightChainArcs(std::vector<std::pair<Math::Vec2, Math::Vec2>>& outArcs) const
{
    constexpr int pairCount = static_cast<int>(sizeof(kStraightCarPairs) / sizeof(kStraightCarPairs[0]));
    for (int e = 0; e < pairCount; ++e)
    {
        const int a = kStraightCarPairs[e][0];
        const int b = kStraightCarPairs[e][1];

        if (!m_carTransportSlots[static_cast<size_t>(a)].engineOn
            || !m_carTransportSlots[static_cast<size_t>(b)].engineOn)
            continue;

        outArcs.push_back({ CarTransportWorldCenter(a), CarTransportWorldCenter(b) });
    }
}

bool Train::IsValveMouseHoverable(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorldPos) const
{
    const Math::Vec2 valveWorld = { MIN_X + m_trainOffset + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
    // Interaction range widened so player can rotate from noticeably farther away.
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, valveWorld, { 560.0f, 320.0f }))
        return false;

    return Collision::CheckPointInAABB(mouseWorldPos, valveWorld, m_valveVisualSize)
        || Collision::CheckAABB(mouseWorldPos, { 32.0f, 32.0f }, valveWorld, m_valveVisualSize);
}

void Train::UpdateValveWaterParticles(float dt)
{
    const float gravityY = -1200.0f;
    const float floorY = MIN_Y - 80.0f;
    const Math::Vec2 valveWorld = { MIN_X + m_trainOffset + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
    std::vector<ValveWaterParticle> splashSpawn;

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
                splashSpawn.push_back(sp);
            }
            p.life = 0.0f;
        }
    }
    m_valveWaterParticles.erase(
        std::remove_if(m_valveWaterParticles.begin(), m_valveWaterParticles.end(),
            [floorY](const ValveWaterParticle& p) { return p.life <= 0.0f || p.pos.y < floorY; }),
        m_valveWaterParticles.end());
    if (!splashSpawn.empty())
        m_valveWaterParticles.insert(m_valveWaterParticles.end(), splashSpawn.begin(), splashSpawn.end());

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
}

void Train::ApplyMooreConnectedPulseShare(Player& player, float dt)
{
    bool visited[kCarTransportCount] = {};
    float bonus                       = 0.f;

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        if (!m_carTransportSlots[static_cast<size_t>(i)].engineOn || visited[i])
            continue;

        std::vector<int> q;
        q.push_back(i);
        visited[i] = true;
        size_t qi = 0;
        int cluster = 0;

        while (qi < q.size())
        {
            const int u = q[qi++];
            ++cluster;
            for (int v = 0; v < kCarTransportCount; ++v)
            {
                if (!m_carTransportSlots[static_cast<size_t>(v)].engineOn || visited[v])
                    continue;
                if (!IsMooreAdjacentCarSlots(u, v))
                    continue;
                visited[v] = true;
                q.push_back(v);
            }
        }

        if (cluster >= 2)
            bonus += static_cast<float>(cluster - 1) * kMoorePulseShareRate * dt;
    }

    if (bonus > 0.f)
        player.GetPulseCore().getPulse().add(bonus);
}

void Train::FireStraightLineChainDetonations()
{
    if (!m_droneManager)
        return;

    constexpr int pairCount = static_cast<int>(sizeof(kStraightCarPairs) / sizeof(kStraightCarPairs[0]));
    for (int e = 0; e < pairCount; ++e)
    {
        const int        a = kStraightCarPairs[e][0];
        const int        b = kStraightCarPairs[e][1];
        if (!m_carTransportSlots[static_cast<size_t>(a)].engineOn
            || !m_carTransportSlots[static_cast<size_t>(b)].engineOn)
            continue;

        const Math::Vec2 wa  = CarTransportWorldCenter(a);
        const Math::Vec2 wb  = CarTransportWorldCenter(b);
        const Math::Vec2 mid = { (wa.x + wb.x) * 0.5f, (wa.y + wb.y) * 0.5f };
        m_droneManager->ApplyDetonation(mid, kChainDetonateRadius, kChainDetonateStun);
    }
}

void Train::UpdateCarTransport(float dt, Player& player, Math::Vec2 playerHbCenter,
                               bool pulseAbsorbHeld, bool ignorePulseCost)
{
    const float fdt = dt;

    for (auto& s : m_carTransportSlots)
    {
        if (s.engineGlowTimer > 0.f)
            s.engineGlowTimer = std::max(0.f, s.engineGlowTimer - fdt);
    }

    ApplyMooreConnectedPulseShare(player, fdt);

    int focus = -1;
    if (pulseAbsorbHeld)
        focus = FindCarTransportInjectTarget(playerHbCenter);

    if (!pulseAbsorbHeld || focus < 0)
    {
        if (m_carInjectFocusSlot >= 0)
        {
            m_carTransportSlots[static_cast<size_t>(m_carInjectFocusSlot)].injectSeconds = 0.f;
            m_carInjectFocusSlot                                                             = -1;
        }
        return;
    }

    if (m_carInjectFocusSlot >= 0 && m_carInjectFocusSlot != focus)
        m_carTransportSlots[static_cast<size_t>(m_carInjectFocusSlot)].injectSeconds = 0.f;

    m_carInjectFocusSlot = focus;
    auto& slot           = m_carTransportSlots[static_cast<size_t>(focus)];

    if (slot.engineOn)
    {
        slot.injectSeconds   = 0.f;
        m_carInjectFocusSlot = -1;
        return;
    }

    Pulse& pulse = player.GetPulseCore().getPulse();

    if (!ignorePulseCost)
    {
        const float need = kCarInjectPulseDrainSec * fdt;
        if (pulse.Value() < need)
        {
            slot.injectSeconds   = 0.f;
            m_carInjectFocusSlot = -1;
            return;
        }
        pulse.spend(need);
    }

    slot.injectSeconds += fdt;
    if (slot.injectSeconds >= kCarInjectDurationSec)
    {
        slot.engineOn      = true;
        slot.injectSeconds = 0.f;
        slot.engineGlowTimer = 1.35f;
        m_carInjectFocusSlot = -1;
        FireStraightLineChainDetonations();
        Logger::Instance().Log(Logger::Severity::Info,
                               "Train: Car transport engine ON (slot %d)", focus);
    }
}


// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
void Train::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "Train Map Initialize");

    m_prevPlayerOnTrain   = false;
    m_airborneFromTrain   = false;
    m_crouchCarryLatched  = false;
    m_prevOnJumpThroughSurface = false;
    m_prevCrouchHeld = false;
    m_pipeDropCooldown = 0.0f;

    // --- Train car images ---
    m_firstTrain  = std::make_unique<Background>();
    m_secondTrain = std::make_unique<Background>();
    m_thirdTrain       = std::make_unique<Background>();
    m_thirdThirdTrain  = std::make_unique<Background>();
    m_fourthTrain      = std::make_unique<Background>();
    m_valveSprite      = std::make_unique<Background>();

    m_firstTrain ->Initialize("Asset/Train/FirstTrain.png");
    m_secondTrain->Initialize("Asset/Train/SecondTrain.png");
    m_thirdTrain ->Initialize("Asset/Train/ThirdTrain.png");
    m_thirdThirdTrain->Initialize("Asset/Train/Third_ThirdTrain.png");
    m_fourthTrain    ->Initialize("Asset/Train/FourthTrain.png");
    // File name in request had spacing typo ("Valve. png"), so try common variants.
    m_valveSprite->Initialize("Asset/Train/Valve.png");
    if (m_valveSprite->GetWidth() <= 0)
        m_valveSprite->Initialize("Asset/Train/Valve. png");

    // Use actual image widths as world widths (1 pixel = 1 world unit)
    if (m_firstTrain->GetWidth()  > 0) m_car1Width = static_cast<float>(m_firstTrain->GetWidth());
    if (m_secondTrain->GetWidth() > 0) m_car2Width = static_cast<float>(m_secondTrain->GetWidth());
    if (m_thirdTrain->GetWidth()  > 0) m_car3Width = static_cast<float>(m_thirdTrain->GetWidth());
    if (m_thirdThirdTrain->GetWidth() > 0) m_car4Width = static_cast<float>(m_thirdThirdTrain->GetWidth());
    if (m_fourthTrain->GetWidth()     > 0) m_car5Width = static_cast<float>(m_fourthTrain->GetWidth());
    m_totalTrainWidth = m_car1Width + m_car2Width + m_car3Width + m_car4Width + m_car5Width;

    // Car5 valve anchor: centered on existing valve/pipe hitbox (c5, 894,351,317,162).
    // Keep local-space so it follows train offset automatically.
    {
        const float c5 = m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        Train::TrainHitbox valveHb = MakeHitbox(c5, 894.0f, 351.0f, 317.0f, 162.0f);
        m_valveLocalCenter = valveHb.localCenter;
    }

    // --- Rail tile ---
    m_railTile = std::make_unique<Background>();
    m_railTile->Initialize("Asset/Train/rail.png");

    if (m_railTile->GetWidth() > 0)
        m_railTileW = static_cast<float>(m_railTile->GetWidth());
    if (m_railTile->GetHeight() > 0)
        m_railTileH = static_cast<float>(m_railTile->GetHeight());

    // How many rail tiles needed to cover train width (plus a few extra for safety)
    m_railTileCount = static_cast<int>(std::ceil(m_totalTrainWidth / m_railTileW)) + 4;

    // --- Map extents (kept for legacy code / config) ---
    m_size     = { m_totalTrainWidth, HEIGHT };
    m_position = { MIN_X + m_totalTrainWidth * 0.5f, MIN_Y + HEIGHT * 0.5f };

    // --- Drone manager ---
    m_droneManager       = std::make_unique<DroneManager>();
    m_sirenDroneManager  = std::make_unique<DroneManager>();

    m_car2EnterPromptTex = std::make_unique<Background>();
    m_car2LeavePromptTex = std::make_unique<Background>();
    m_car2EnterPromptTex->Initialize("Asset/Train/Enter.png");
    m_car2LeavePromptTex->Initialize("Asset/Train/Leave.png");

    // --- Build train hitboxes from known pixel coordinates ---
    BuildTrainHitboxes();
    ResetCarTransportSlotsToInitialState();

    // --- Sky gradient VAO ---
    InitSkyVAO();
    InitValveWaterGpu();

    // --- Train state ---
    m_trainState      = TrainState::Stationary;
    m_trainOffset     = 0.0f;
    m_trainCurrentSpeed = 0.0f;
    m_entryTimer      = -1.0f; // not started until StartEntryTimer() is called
    m_departedMsgTimer = 0.0f;
    m_playerOnTrain   = false;

    // Train departure / running sounds.
    // User requested "TrainStart.mpe" and "TrainSound.mp3". Keep .mpe first, then fallback to .mp3.
    bool startLoaded = m_trainStartSound.Load("Asset/TrainStart.mpe", false);
    if (!startLoaded)
        startLoaded = m_trainStartSound.Load("Asset/TrainStart.mp3", false);
    bool runLoaded = m_trainRunLoopSound.Load("Asset/TrainSound.mp3", true);
    if (!runLoaded)
        runLoaded = m_trainRunLoopSound.Load("Asset/TrainSound.mpe", true);
    if (runLoaded)
        m_trainRunLoopSound.SetVolume(0.0f);

    ApplyConfig(MapObjectConfig::Instance().GetData().train);

    // 드론 10 / 로봇 3 — FourthTrain(물탱크) 구간 스크립트 전제 배치 (플레이어가 Car5 발판에 올라올 때까지 정지)
    {
        for (auto& r : m_robots)
            r.Shutdown();
        m_robots.clear();
        if (m_droneManager)
            m_droneManager->ClearAllDrones();

        const float        c5      = m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const TrainHitbox  deck5   = MakeHitbox(c5, 84, 804, 2472, 45);

        constexpr float kLowDroneY = Train::MIN_Y + 95.f + 88.f;
        const float kDroneXLocs[10] = {
            c5 + 160.f,  c5 + 420.f,  c5 + 620.f,  c5 + 860.f,  c5 + 1080.f,
            c5 + 1320.f, c5 + 1540.f, c5 + 1760.f, c5 + 1980.f, c5 + 2180.f
        };

        for (int i = 0; i < 10; ++i)
        {
            const float x = MIN_X + kDroneXLocs[i];
            const float y = kLowDroneY + std::sin(static_cast<float>(i) * 1.55f) * 18.f;
            m_droneManager->SpawnDrone({ x, y }, "Asset/Drone.png", false).SetBaseSpeed(95.f);
        }

        const float railFoot = Train::MIN_Y + 95.f;
        for (int i = 0; i < 3; ++i)
        {
            m_robots.emplace_back();
            Robot& rr = m_robots.back();
            rr.Init({ MIN_X + c5 + 400.f, railFoot + 120.f });
            const float cx = MIN_X + c5 + 520.f + static_cast<float>(i) * 260.f;
            const float cy = railFoot + rr.GetSize().y * 0.5f;
            rr.SetPosition({ cx, cy });
            rr.ApplyUndergroundDifficultyBoost();
            rr.SetGroundLimitY(Train::MIN_Y + 85.f);
        }

        m_droneWaterCd.assign(m_droneManager->GetDrones().size(), 0.f);
        m_robotWaterCd.assign(m_robots.size(), 0.f);
    }

    m_prevTrainOffsetActors = m_trainOffset;
    m_car5EncounterActive     = false;
    m_encounterScriptTime     = 0.f;

    Logger::Instance().Log(Logger::Severity::Info,
        "Train Map initialized – car widths: %.0f / %.0f / %.0f / %.0f / %.0f (total %.0f), rail tile: %.0f — drones %zu robots %zu",
        m_car1Width, m_car2Width, m_car3Width, m_car4Width, m_car5Width, m_totalTrainWidth, m_railTileW,
        m_droneManager->GetDrones().size(), m_robots.size());
}


// ---------------------------------------------------------------------------
// BuildTrainHitboxes
//
//  MakeHitbox( carOffset, pixelX, pixelY, width, height )
//
//    carOffset  : c1 / c2 / c3  – 열차칸 시작 X (자동 계산, 건드리지 말 것)
//    pixelX     : 해당 오브젝트의 이미지 내 좌측 X 픽셀 위치 (← → 조정)
//    pixelY     : 해당 오브젝트의 이미지 내 상단 Y 픽셀 위치 (↑ 작을수록 위, ↓ 클수록 아래)
//    width      : 히트박스 가로 크기 (픽셀 단위)
//    height     : 히트박스 세로 크기 (픽셀 단위)
//
//  ※ 디버그 모드에서 cyan 박스로 확인 가능
//  ※ 이미지 좌표 기준 (Y=0 이 이미지 최상단), 세계 좌표로 자동 변환됨
// ---------------------------------------------------------------------------
void Train::BuildTrainHitboxes()
{
    m_trainHitboxes.clear();
    m_car5DeckHbValid = false;

    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 1  –  FirstTrain.png
    //    이미지 내 오브젝트 배치 (좌 → 우)
    //    [발판] - [컨테이너A(빨강)] - [컨테이너B(황갈, A 위)] - [컨테이너C(황갈)] - [컨테이너D(은색, 우측)]
    // ════════════════════════════════════════════════════════════════════════
    const float c1 = 0.0f; // Car 1 시작 X (열차 맨 앞)

    // [Car1] 발판 (열차 바닥 플랫폼) ─ 플레이어가 올라서는 주요 발판
    //   위치: X=84  Y=804  크기: 2472 x 45
    //   ※ 세 열차 공통 – 높이(Y)나 두께(height) 조정 시 세 곳 모두 동일하게 변경
    m_trainHitboxes.push_back(MakeHitbox(c1,   84,  804, 2472,  45));

    // [Car1] 컨테이너 A  ─  좌측 빨간 컨테이너 (발판 위, 낮은 층)
    //   위치: X=363  Y=519  크기: 189 x 286
    m_trainHitboxes.push_back(MakeHitbox(c1,  363,  519,  189, 286));

    // [Car1] 컨테이너 B  ─  황갈색 대형 컨테이너 (A 위에 적재, 높은 층)
    //   위치: X=552  Y=237  크기: 660 x 282
    //   ※ 위에 올라갈 수 있는 가장 높은 지점 중 하나
    m_trainHitboxes.push_back(MakeHitbox(c1,  552,  237,  660, 282));

    // [Car1] 컨테이너 C  ─  중앙 황갈색 컨테이너 (발판 위, 낮은 층)
    //   위치: X=635  Y=519  크기: 540 x 285
    m_trainHitboxes.push_back(MakeHitbox(c1,  1212,  519,  540, 285));

    // [Car1] 컨테이너 D  ─  우측 은색 대형 컨테이너 (히딩박스 – 충돌 없음, 내부 진입 가능)
    //   위치: X=1806  Y=531  크기: 705 x 273  →  m_hidingSpots에만 등록


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 2  –  SecondTrain.png
    //    [발판] - [컨테이너E(빨강)] - [컨테이너F(어두운 소형)] - [컨테이너G(보라, 위)] - [컨테이너H(황갈, 우측)]
    // ════════════════════════════════════════════════════════════════════════
    const float c2 = m_car1Width; // Car 2 시작 X (Car 1 끝 지점)

    // [Car2] 발판 (Car 1 발판과 동일 상대 위치)
    //   위치: X=84  Y=804  크기: 2472 x 45
    m_trainHitboxes.push_back(MakeHitbox(c2,   84,  804, 2472,  45));

    // [Car2] 컨테이너 E  ─  좌측 적갈색 컨테이너 (발판 위, 낮은 층)
    //   위치: X=285  Y=519  크기: 621 x 285
    m_trainHitboxes.push_back(MakeHitbox(c2,  285,  519,  621, 285));

    // [Car2] 컨테이너 F  ─  소형 어두운 컨테이너 (발판 위, 낮은 층)
    //   위치: X=1197  Y=519  크기: 234 x 286
    m_trainHitboxes.push_back(MakeHitbox(c2, 1197,  519,  234, 286));

    // [Car2] 컨테이너 G  ─  대형 보라색 컨테이너 (높은 층 – 가장 높은 위치 중 하나)
    //   위치: X=1431  Y=234  크기: 795 x 285
    //   ※ 위에 올라갈 수 있는 가장 높은 지점
    {
        TrainHitbox purple = MakeHitbox(c2, 1431, 234, 795, 285);
        m_car2PurpleHb      = purple;
        m_car2PurpleHbValid = true;
        m_trainHitboxes.push_back(purple);
    }

    // [Car2] 컨테이너 H  ─  우측 소형 황갈색 컨테이너 (발판 위, 낮은 층)
    //   위치: X=2324  Y=519  크기: 261 x 285
    m_trainHitboxes.push_back(MakeHitbox(c2, 2226,  519,  261, 285));


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 3  –  ThirdTrain.png
    //    [발판] - [컨테이너I(은색/흰)] - [컨테이너J(어두운 대형)] - [컨테이너K(황갈, 위)] - [컨테이너L(소형, 우측)]
    // ════════════════════════════════════════════════════════════════════════
    const float c3 = m_car1Width + m_car2Width; // Car 3 시작 X (Car 2 끝 지점)

    // [Car3] 사이렌 장치 — 충돌 없음 (펄스 주입 상호작용 전용)
    {
        TrainHitbox siren = MakeHitbox(c3, 1644.f, 207.f, 126.f, 163.f, false);
        m_car3SirenHb      = siren;
        m_car3SirenHbValid = true;
        m_trainHitboxes.push_back(siren);
    }

    // [Car3] 발판 (Car 1 발판과 동일 상대 위치)
    //   위치: X=84  Y=804  크기: 2472 x 45
    m_trainHitboxes.push_back(MakeHitbox(c3,   84,  804, 2472,  45));

    // [Car3] 컨테이너 I  ─  좌측 은색/흰색 컨테이너 (히딩박스 – 충돌 없음, 내부 진입 가능)
    //   위치: X=330  Y=504  크기: 378 x 300  →  m_hidingSpots에만 등록

    // [Car3] 컨테이너 J  ─  중앙 대형 어두운 컨테이너 (발판 위, 낮은 층)
    //   위치: X=1116  Y=519  크기: 534 x 285
    m_trainHitboxes.push_back(MakeHitbox(c3, 1116,  519,  534, 285));

    // [Car3] 컨테이너 K  ─  우측 황갈색 컨테이너 (높은 층 – J 위에 적재)
    //   위치: X=1650  Y=252  크기: 660 x 267
    //   ※ 위에 올라갈 수 있는 높은 지점
    m_trainHitboxes.push_back(MakeHitbox(c3, 1650,  252,  660, 267));

    // [Car3] 컨테이너 L  ─  최우측 소형 컨테이너 (발판 위, 낮은 층)
    //   위치: X=2310  Y=519  크기: 126 x 285
    m_trainHitboxes.push_back(MakeHitbox(c3, 2310,  519,  126, 285));


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 4  –  Third_ThirdTrain.png
    //    발판만 충돌 — 자동차 박스는 디버그 표시 전용(collision false), 앞으로 걸어 통과
    // ════════════════════════════════════════════════════════════════════════
    const float c4 = m_car1Width + m_car2Width + m_car3Width;

    // [Car4] 발판  위치: X=84  Y=804  크기: 3789 x 45
    m_trainHitboxes.push_back(MakeHitbox(c4, 84, 804, 3789, 45));

    // [Car4] 3층·2층 파이프 — 층당 1개의 긴 연속 파이프
    constexpr float kPipeH = 18.0f;

    // 3층 파이프: X=429 ~ X=3515 (왼쪽 차 시작 ~ 오른쪽 차 끝), 폭 = 3086
    m_trainHitboxes.push_back(MakeHitbox(c4, 429.0f, 74.0f, 3086.0f, kPipeH, true,
                                         Train::TrainHitboxKind::JumpThroughPipe));

    // 2층 파이프: X=432 ~ X=3507 (왼쪽 차 시작 ~ 오른쪽 차 끝), 폭 = 3075
    m_trainHitboxes.push_back(MakeHitbox(c4, 432.0f, 434.0f, 3075.0f, kPipeH, true,
                                         Train::TrainHitboxKind::JumpThroughPipe));

    constexpr bool kCar4VisualOnly = false;
    AppendCarSilhouette(m_trainHitboxes, c4, 429,  129, 886, 304, kCar4VisualOnly); // top-left
    AppendCarSilhouette(m_trainHitboxes, c4, 1536, 129, 886, 304, kCar4VisualOnly); // top-middle
    AppendCarSilhouette(m_trainHitboxes, c4, 2629, 129, 886, 304, kCar4VisualOnly); // top-right

    AppendCarSilhouette(m_trainHitboxes, c4, 432,  489, 876, 306, kCar4VisualOnly); // bottom-left
    AppendCarSilhouette(m_trainHitboxes, c4, 1539, 489, 876, 306, kCar4VisualOnly); // bottom-middle
    AppendCarSilhouette(m_trainHitboxes, c4, 2631, 489, 876, 306, kCar4VisualOnly); // bottom-right


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 5  –  FourthTrain.png (탱크 + 밸브)
    // ════════════════════════════════════════════════════════════════════════
    const float c5 = c4 + m_car4Width;

    // [Car5] 발판  위치: X=84  Y=804  크기: 2472 x 45
    {
        TrainHitbox deck = MakeHitbox(c5, 84, 804, 2472, 45);
        m_trainHitboxes.push_back(deck);
        m_car5DeckHb     = deck;
        m_car5DeckHbValid = true;
    }

    m_trainHitboxes.push_back(MakeHitbox(c5, 342, 498, 63, 307));   // tank left end
    m_trainHitboxes.push_back(MakeHitbox(c5, 405, 498, 948, 307)); // tank main body
    m_trainHitboxes.push_back(MakeHitbox(c5, 1353, 498, 62, 307)); // tank right end
    m_trainHitboxes.push_back(MakeHitbox(c5, 894, 351, 317, 162));  // valve / pipe on top


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Hiding Spots  (기차와 함께 이동, 드론 탐지 차단)
    //    Car1 D / Car3 I  (Car4 자동차 윤곽은 비충돌 표시만)
    // ════════════════════════════════════════════════════════════════════════
    m_hidingSpots.clear();

    // [Car1] 컨테이너 D  ─  우측 은색 대형 컨테이너
    {
        auto hs = MakeHitbox(c1, 1806, 531, 705, 273);
        m_hidingSpots.push_back({ hs.localCenter, hs.size });
    }

    // [Car3] 컨테이너 I  ─  좌측 은색/흰색 컨테이너
    {
        auto hs = MakeHitbox(c3, 330, 504, 378, 300);
        m_hidingSpots.push_back({ hs.localCenter, hs.size });
    }
}


// ---------------------------------------------------------------------------
// Valve water — GPU instanced rendering (single batched draw)
// ---------------------------------------------------------------------------
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

void Train::ApplyValveWaterDamageToEnemies(float dt)
{
    if (dt <= 0.0f)
        return;
    if (m_valvePressureT <= 0.01f && m_valveWaterParticles.empty())
        return;

    const float valveY = MIN_Y + m_valveLocalCenter.y;
    const float tl       = MIN_X + m_trainOffset;
    const float valveX   = tl + m_valveLocalCenter.x;
    const float trainRight = tl + m_totalTrainWidth;

    auto hitsWater = [&](const Math::Vec2& enemyCenter, const Math::Vec2& enemySize, float extraEnemyPadding) -> bool
    {
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

    for (size_t i = 0; i < m_robots.size(); ++i)
    {
        auto& r = m_robots[i];
        if (r.IsDead())
            continue;

        if (i < m_robotWaterCd.size() && m_robotWaterCd[i] > 0.0f)
            m_robotWaterCd[i] -= dt;

        if (i < m_robotWaterCd.size() && m_robotWaterCd[i] > 0.0f)
            continue;

        const bool wet = hitsWater(r.GetPosition(), r.GetSize(), 28.0f)
                         || inSprayBand(r.GetPosition(), r.GetSize());
        if (!wet)
            continue;

        r.SetPosition(r.GetPosition() + Math::Vec2{ 220.f, 0.f });
        r.TakeDamage(r.GetMaxHP() * 0.80f);
        if (i < m_robotWaterCd.size())
            m_robotWaterCd[i] = 1.15f;
    }
}


void Train::ApplyTrainMotionToDronesAndRobots(float deltaTrainX)
{
    if (deltaTrainX == 0.0f)
        return;

    if (m_droneManager)
    {
        for (auto& d : m_droneManager->GetDrones())
            d.SetPosition(d.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
    }

    if (m_sirenDroneManager)
    {
        for (auto& d : m_sirenDroneManager->GetDrones())
            d.SetPosition(d.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
    }

    for (auto& r : m_robots)
        r.SetPosition(r.GetPosition() + Math::Vec2{ deltaTrainX, 0.f });
}


bool Train::IsPlayerOnCar5Deck(Math::Vec2 hbCenter, Math::Vec2 hbSize) const
{
    if (!m_car5DeckHbValid)
        return false;

    const float        tl       = MIN_X + m_trainOffset;
    Math::Vec2 deckWorld = { tl + m_car5DeckHb.localCenter.x, MIN_Y + m_car5DeckHb.localCenter.y };
    Math::Vec2 deckSize = m_car5DeckHb.size;
    // 착지·판정 안정화: 발판 위로 약간 확장 (히트박스가 발판과 스킵되는 경우 방지)
    deckSize.y += 72.f;
    deckWorld.y += 36.f;
    return Collision::CheckAABB(hbCenter, hbSize, deckWorld, deckSize);
}


void Train::TryActivateCar5Encounter(Math::Vec2 hbCenter, Math::Vec2 hbSize)
{
    if (m_car5EncounterActive)
        return;

    bool shouldActivate = IsPlayerOnCar5Deck(hbCenter, hbSize);
    if (!shouldActivate)
    {
        // Deck top 외에도 탱크/밸브 위에 올라탄 경우 활성화되도록 Car5 구역 체크를 허용.
        const float tl = MIN_X + m_trainOffset;
        const float c5 = m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float car5L = tl + c5 - 30.f;
        const float car5R = tl + c5 + m_car5Width + 30.f;
        shouldActivate = (hbCenter.x >= car5L && hbCenter.x <= car5R &&
                          hbCenter.y >= MIN_Y + 120.f && hbCenter.y <= MIN_Y + HEIGHT + 260.f);
    }
    if (!shouldActivate)
        return;

    m_car5EncounterActive = true;
    m_encounterScriptTime = 0.f;

    // 로봇을 레일 바닥 근처에 두고(Robot::Update — Underground AI) 벽·차체에 막히면 점프해 탑승.
    if (!m_robots.empty())
    {
        const float tl   = MIN_X + m_trainOffset;
        const float c5L  = tl + m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float rail = Train::MIN_Y + 95.f;
        for (size_t i = 0; i < m_robots.size(); ++i)
        {
            Robot& rr = m_robots[i];
            const float cx = c5L + 280.f + static_cast<float>(i) * 240.f;
            const float cy = rail + rr.GetSize().y * 0.5f;
            rr.SetPosition({ cx, cy });
            rr.SetGroundLimitY(Train::MIN_Y + 85.f);
        }
    }
    Logger::Instance().Log(Logger::Severity::Event, "Train: FourthTrain deck encounter activated");
}


void Train::AssistRobotRailJumpTowardTrain(Robot& robot, const Player& player, float dt)
{
    (void)dt;
    if (robot.IsDead())
        return;

    const Math::Vec2 pp = player.GetHitboxCenter();
    const Math::Vec2 rp = robot.GetPosition();
    const float      feet = rp.y - robot.GetSize().y * 0.5f;

    if (feet > Train::MIN_Y + 230.f)
        return;
    if (pp.y < rp.y + 90.f)
        return;
    if (robot.GetVelocity().y > 320.f)
        return;

    const RobotState st = robot.GetState();
    if (st != RobotState::Chase && st != RobotState::Patrol && st != RobotState::Windup && st != RobotState::Recover)
        return;

    if (std::abs(robot.GetVelocity().x) > 40.f)
        return;

    Math::Vec2 v = robot.GetVelocity();
    v.y          = 980.f;
    robot.SetVelocity(v);
}


void Train::UpdateTrainRobotsAI(float dt, Player& player)
{
    if (!m_car5EncounterActive || m_robots.empty())
        return;

    const float tl        = MIN_X + m_trainOffset;
    const float trainRight = tl + m_totalTrainWidth;
    const bool  fleeWater = (m_valvePressureT > 0.10f);

    std::vector<ObstacleInfo> obstacleInfos;
    obstacleInfos.reserve(m_trainHitboxes.size());
    for (const auto& hb : m_trainHitboxes)
    {
        if (!hb.collision)
            continue;
        Math::Vec2 world = { tl + hb.localCenter.x, MIN_Y + hb.localCenter.y };
        obstacleInfos.push_back({ world, hb.size });
    }

    const float mapMinX = Train::MIN_X + m_trainOffset - 500.f;
    const float mapMaxX = trainRight + 900.f;

    for (auto& r : m_robots)
    {
        if (r.IsDead())
            continue;

        r.SetGroundLimitY(Train::MIN_Y + 85.f);

        if (fleeWater)
        {
            Math::Vec2 p = r.GetPosition();
            float      nx = p.x + 310.f * dt;
            const float cap = trainRight - 115.f;
            if (nx > cap)
                nx = cap;
            p.x = nx;
            r.SetPosition(p);
            continue;
        }

        r.Update(dt, player, obstacleInfos, mapMinX, mapMaxX);
        AssistRobotRailJumpTowardTrain(r, player, dt);
    }
}


void Train::UpdateCar3Siren(float dt, Player& player, Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                            Math::Vec2 mouseWorldPos, bool attackHeld, bool injectGodMode)
{
    if (!m_car3SirenHbValid || !m_sirenDroneManager)
        return;

    if (m_car3SirenPendingShutdown)
    {
        m_car3SirenPendingShutdown = false;
        m_car3SirenActive          = false;
    }

    constexpr float kInjectPulseTotal  = 5.f;
    constexpr float kInjectDurationSec = 1.5f;
    constexpr float kPostSirenChaseMul = 0.48f; // 사이렌 정지 후 추적 속도 배율

    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 sirenW = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };

    m_car3SirenWaveAnim += dt * 5.5f;

    const bool inInjectRange =
        Collision::CheckAABB(playerHbCenter, playerHitboxSize, sirenW, { 380.f, 300.f });
    const bool cursorOnSiren = Collision::CheckPointInAABB(mouseWorldPos, sirenW, m_car3SirenHb.size);

    if (m_car3SirenActive)
    {
        m_car3SirenSpawnTimer += dt;
        if (m_car3SirenSpawnTimer >= 2.4f && m_sirenDroneManager->GetDrones().size() < 14)
        {
            m_car3SirenSpawnTimer = 0.f;
            const Math::Vec2 spawnPos = { sirenW.x + 25.f, sirenW.y + 55.f };
            Drone& d = m_sirenDroneManager->SpawnDrone(spawnPos, "Asset/Drone.png", true);
            d.SetBaseSpeed(185.f);
            d.SetSirenMapDrone(true);
        }

        if (attackHeld && inInjectRange && cursorOnSiren)
        {
            const float pulsePerSec    = kInjectPulseTotal / kInjectDurationSec;
            float       pulseThisFrame = pulsePerSec * static_cast<float>(dt);
            if (!injectGodMode)
            {
                pulseThisFrame = std::min(pulseThisFrame, player.GetPulseCore().getPulse().Value());
                if (pulseThisFrame > 0.f)
                    player.GetPulseCore().getPulse().spend(pulseThisFrame);
            }
            if (pulseThisFrame > 0.f || injectGodMode)
            {
                const float deltaT = injectGodMode ? (static_cast<float>(dt) / kInjectDurationSec)
                                                   : (pulseThisFrame / kInjectPulseTotal);
                m_car3SirenInjectT += deltaT;
            }
            if (m_car3SirenInjectT >= 1.f)
            {
                m_car3SirenInjectT           = 1.f;
                m_car3SirenPendingShutdown    = true;
                m_car3SirenSpawnTimer        = 0.f;
            }
        }
    }

    const bool hideForSiren =
        IsPlayerHiding(playerHbCenter, playerHitboxSize, player.IsCrouching())
        || IsPlayerInCar2PurplePulseBox(playerHbCenter, playerHitboxSize);
    const float chaseMul = m_car3SirenActive ? 1.f : kPostSirenChaseMul;
    m_sirenDroneManager->Update(dt, player, playerHitboxSize, hideForSiren, true, chaseMul);
}


bool Train::IsCar3SirenMouseHoverForPulseInject(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize,
                                                Math::Vec2 mouseWorld) const
{
    if (!m_car3SirenHbValid || !m_car3SirenActive)
        return false;
    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 sirenW = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };
    if (!Collision::CheckAABB(playerHbCenter, playerHbSize, sirenW, { 380.f, 300.f }))
        return false;
    return Collision::CheckPointInAABB(mouseWorld, sirenW, m_car3SirenHb.size);
}


bool Train::IsPlayerInCar2PurplePulseBox(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize) const
{
    if (!m_car2PurpleHbValid || m_car2HidePhase != Car2HidePhase::Inside)
        return false;
    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };
    return Collision::CheckAABB(playerHbCenter, playerHitboxSize, contC, m_car2PurpleHb.size);
}


bool Train::IsSirenDroneDamageBlocked(Math::Vec2 playerHbCenter, Math::Vec2 playerHitboxSize,
                                      bool isPlayerCrouching) const
{
    return IsPlayerHiding(playerHbCenter, playerHitboxSize, isPlayerCrouching)
        || IsPlayerInCar2PurplePulseBox(playerHbCenter, playerHitboxSize);
}


bool Train::IsCar2PurpleHitbox(const TrainHitbox& hb) const
{
    if (!m_car2PurpleHbValid)
        return false;
    constexpr float eps = 1.5f;
    return std::abs(hb.localCenter.x - m_car2PurpleHb.localCenter.x) < eps
        && std::abs(hb.localCenter.y - m_car2PurpleHb.localCenter.y) < eps
        && std::abs(hb.size.x - m_car2PurpleHb.size.x) < eps
        && std::abs(hb.size.y - m_car2PurpleHb.size.y) < eps;
}


void Train::UpdateCar2PurpleContainer(float dt, Player& player, Math::Vec2 mouseWorldPos, bool attackTriggered,
                                      bool pulseAbsorbHeld, bool injectGodMode)
{
    if (!m_car2PurpleHbValid)
        return;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };

    // 플레이어 히트박스 중심을 컨테이너 중심에 고정 — 중력/걷기로 박스 아래로 떨어지지 않음
    auto snapHitboxCenterTo = [&](Math::Vec2 worldHbCenter) {
        const Math::Vec2 hc = player.GetHitboxCenter();
        player.SetPosition(player.GetPosition() + (worldHbCenter - hc));
    };

    const bool nearPurple = Collision::CheckAABB(player.GetHitboxCenter(), player.GetHitboxSize(), contC,
                                                 { m_car2PurpleHb.size.x + 520.f, m_car2PurpleHb.size.y + 520.f });

    const Math::Vec2 promptCenter = { contC.x - m_car2PurpleHb.size.x * 0.5f - 115.f, contC.y + 10.f };
    const Math::Vec2 promptSize   = { 150.f, 78.f };
    const bool       hoverPrompt =
        Collision::CheckPointInAABB(mouseWorldPos, promptCenter, promptSize) && nearPurple;

    auto smoothstep = [](float t) -> float {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    };

    switch (m_car2HidePhase)
    {
    case Car2HidePhase::None:
        player.SetTrainCar2ForcedCrouch(false);
        player.SetMovementLockedByTrain(false);
        player.SetCar2LeaveWalk(false);
        player.SetSpriteAlphaMul(1.f);
        if (nearPurple && attackTriggered && hoverPrompt && m_car2InsideCharge < 99.5f)
        {
            m_car2EnterSavedValid   = true;
            m_car2EnterPlayerWorld  = player.GetPosition();
            m_car2EnterTrainOffset  = m_trainOffset;
            m_car2HidePhase         = Car2HidePhase::Entering;
            m_car2HideSeqTime       = 0.f;
            player.SetCar2LeaveWalk(false);
            snapHitboxCenterTo(contC);
        }
        break;

    case Car2HidePhase::Entering:
    {
        player.SetTrainCar2ForcedCrouch(false);
        player.SetMovementLockedByTrain(true);
        player.SetCar2LeaveWalk(false);
        snapHitboxCenterTo(contC);

        m_car2HideSeqTime += dt;
        const float dur = 0.55f;
        float       u   = smoothstep(std::min(1.f, m_car2HideSeqTime / dur));
        player.SetSpriteAlphaMul(1.f - 0.62f * u);

        if (m_car2HideSeqTime >= dur)
        {
            m_car2HidePhase   = Car2HidePhase::Inside;
            m_car2HideSeqTime = 0.f;
            snapHitboxCenterTo(contC);
            player.SetSpriteAlphaMul(0.38f);
            player.SetTrainCar2ForcedCrouch(true);
        }
    }
    break;

    case Car2HidePhase::Inside:
    {
        player.SetMovementLockedByTrain(true);
        player.SetCar2LeaveWalk(false);
        player.SetTrainCar2ForcedCrouch(true);
        snapHitboxCenterTo(contC);
        player.SetSpriteAlphaMul(0.38f);

        float chargeMeterPerSec = 30.f;
        float pulsePerSec       = 40.f;
        if (pulseAbsorbHeld)
        {
            chargeMeterPerSec = 48.f;
            pulsePerSec       = 72.f;
        }
        player.GetPulseCore().getPulse().add(pulsePerSec * static_cast<float>(dt));
        m_car2InsideCharge = std::min(100.f, m_car2InsideCharge + chargeMeterPerSec * static_cast<float>(dt));

        if (nearPurple && attackTriggered && hoverPrompt && m_car2InsideCharge >= 99.5f)
        {
            player.SetTrainCar2ForcedCrouch(false);
            player.SetCar2LeaveWalk(false);
            if (m_car2EnterSavedValid)
            {
                const float dOff = m_trainOffset - m_car2EnterTrainOffset;
                player.SetPosition({ m_car2EnterPlayerWorld.x + dOff, m_car2EnterPlayerWorld.y });
                m_car2EnterSavedValid = false;
            }
            player.ResetVelocity();
            player.SetOnGround(true);
            player.SetMovementLockedByTrain(false);
            m_car2HidePhase    = Car2HidePhase::None;
            m_car2InsideCharge = 0.f;
            player.SetSpriteAlphaMul(1.f);
        }
    }
    break;
    }
}


void Train::DrawCar3SirenWaves(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car3SirenHbValid || !m_car3SirenActive || m_car3SirenInjectT >= 0.995f)
        return;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 o = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y + 70.f };

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (o.x < visLeft || o.x > visRight)
        return;

    for (int i = 0; i < 4; ++i)
    {
        const float ph = m_car3SirenWaveAnim * 2.6f + static_cast<float>(i) * 1.15f;
        const float wave = std::fmod(ph, 6.2831853f);
        const float rad  = 90.f + static_cast<float>(i) * 68.f + std::sin(wave) * 22.f;
        const float a    = 0.14f + static_cast<float>(i) * 0.05f;
        DrawFilledQuad(colorShader, o, { rad * 2.2f, rad * 0.65f }, 0.55f, 0.15f, 0.95f, a * (1.f - m_car3SirenInjectT));
    }
}


void Train::DrawCar2EnterLeavePrompt(Shader& textureShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car2PurpleHbValid)
        return;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };

    const Math::Vec2 promptCenter = { contC.x - m_car2PurpleHb.size.x * 0.5f - 115.f, contC.y + 10.f };

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (promptCenter.x < visLeft || promptCenter.x > visRight)
        return;

    Background* tex = nullptr;
    if (m_car2HidePhase == Car2HidePhase::Inside && m_car2InsideCharge >= 99.5f && m_car2LeavePromptTex
        && m_car2LeavePromptTex->GetWidth() > 0)
        tex = m_car2LeavePromptTex.get();
    else if ((m_car2HidePhase == Car2HidePhase::None || m_car2HidePhase == Car2HidePhase::Entering)
             && m_car2InsideCharge < 99.5f && m_car2EnterPromptTex && m_car2EnterPromptTex->GetWidth() > 0)
        tex = m_car2EnterPromptTex.get();

    if (!tex)
        return;

    const float pw = static_cast<float>(tex->GetWidth());
    const float ph = static_cast<float>(tex->GetHeight());

    textureShader.use();
    textureShader.setBool("flipX", false);
    textureShader.setFloat("alpha", 1.0f);
    textureShader.setVec3("colorTint", 1.0f, 1.0f, 1.0f);
    textureShader.setFloat("tintStrength", 0.0f);
    textureShader.setVec4("spriteRect", 0.0f, 0.0f, 1.0f, 1.0f);

    Math::Matrix model =
        Math::Matrix::CreateTranslation(promptCenter) * Math::Matrix::CreateScale({ pw * 0.85f, ph * 0.85f });
    tex->Draw(textureShader, model);
}


bool Train::IsCar2EnterLeavePromptHovered(Math::Vec2 playerHbCenter, Math::Vec2 playerHbSize, Math::Vec2 mouseWorld,
                                         Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car2PurpleHbValid)
        return false;

    const float      tl    = MIN_X + m_trainOffset;
    const Math::Vec2 contC = { tl + m_car2PurpleHb.localCenter.x, MIN_Y + m_car2PurpleHb.localCenter.y };

    const bool nearPurple = Collision::CheckAABB(playerHbCenter, playerHbSize, contC,
                                                   { m_car2PurpleHb.size.x + 520.f, m_car2PurpleHb.size.y + 520.f });

    const Math::Vec2 promptCenter = { contC.x - m_car2PurpleHb.size.x * 0.5f - 115.f, contC.y + 10.f };
    const Math::Vec2 promptSize   = { 150.f, 78.f };

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (promptCenter.x < visLeft || promptCenter.x > visRight)
        return false;

    return nearPurple && Collision::CheckPointInAABB(mouseWorld, promptCenter, promptSize);
}


void Train::DrawCar3SirenProgressGauge(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_car3SirenHbValid || !m_car3SirenActive || !m_skyVAO)
        return;

    const float      tl     = MIN_X + m_trainOffset;
    const Math::Vec2 posC = { tl + m_car3SirenHb.localCenter.x, MIN_Y + m_car3SirenHb.localCenter.y };
    const Math::Vec2 half = m_car3SirenHb.size * 0.5f;

    // PulseSource::DrawRemainGauge 와 동일한 배치(오브젝트 오른쪽 세로 바)
    const float barW            = 10.0f;
    const float barH            = std::min(110.0f, std::max(44.0f, m_car3SirenHb.size.y * 0.9f));
    const float padY            = 10.0f;
    const float horizNudgeRight = 38.0f;
    const float vertNudgeDown   = -8.0f;

    const float rx = posC.x + half.x;
    const float ty = posC.y + half.y;
    const float barCenterX = rx - 10.0f - barW * 0.5f + horizNudgeRight;
    const float innerTopY    = ty + padY + vertNudgeDown;
    const float innerCenterY = innerTopY - barH * 0.5f;

    const float visLeft  = cameraPos.x - viewHalfW - 400.f;
    const float visRight = cameraPos.x + viewHalfW + 400.f;
    if (barCenterX < visLeft || barCenterX > visRight)
        return;

    colorShader.use();

    const Math::Vec2 bgSize = { barW + 6.0f, barH + 6.0f };
    DrawFilledQuad(colorShader, { barCenterX, innerCenterY }, bgSize, 0.2f, 0.2f, 0.2f, 1.0f);

    DrawFilledQuad(colorShader, { barCenterX, innerCenterY }, { barW, barH }, 0.14f, 0.14f, 0.18f, 1.0f);

    const float t      = std::clamp(m_car3SirenInjectT, 0.f, 1.f);
    const float fillH  = barH * t;
    if (fillH > 0.5f)
    {
        const float innerBottomY = innerTopY - barH;
        const float fillCenterY  = innerBottomY + fillH * 0.5f;
        DrawFilledQuad(colorShader, { barCenterX, fillCenterY }, { barW, fillH }, 0.8f, 0.2f, 1.0f, 1.0f);
    }
}


void Train::UpdateTrainEncounterScript(float dt, Player& player)
{
    if (!m_car5EncounterActive)
        return;

    const float                 tl        = MIN_X + m_trainOffset;
    const float                 trainRight = tl + m_totalTrainWidth;
    const bool                  fleeWater = (m_valvePressureT > 0.10f);
    const Math::Vec2          ppos      = player.GetHitboxCenter();
    constexpr float             kApproach = 102.f;

    if (m_droneManager)
    {
        auto& drones = m_droneManager->GetDrones();
        const float c5L = tl + m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float c5R = c5L + m_car5Width;
        const float formationCenter = std::clamp(ppos.x, c5L + 260.f, c5R - 260.f);
        const float formationSpacing = 150.f;
        for (size_t i = 0; i < drones.size(); ++i)
        {
            auto& d = drones[i];
            if (d.IsDead())
                continue;

            Math::Vec2 pos = d.GetPosition();
            const float bob =
                std::sin(m_encounterScriptTime * 2.45f + static_cast<float>(i) * 0.73f) * 22.f;
            const float lowY = Train::MIN_Y + 95.f + 82.f + bob;

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
                const float slot = static_cast<float>(i) - static_cast<float>(drones.size() - 1) * 0.5f;
                const float desiredX = std::clamp(formationCenter + slot * formationSpacing, c5L + 120.f, c5R - 120.f);
                dx = std::clamp(desiredX - pos.x, -kApproach * dt, kApproach * dt);
            }

            pos.x += dx;
            pos.y = lowY;
            d.SetPosition(pos);
            d.SetVelocity({ 0.f, 0.f });
            d.SetBaseSpeed(95.f);
        }
    }

    UpdateTrainRobotsAI(dt, player);
}


// ---------------------------------------------------------------------------
// InitSkyVAO – centred unit quad for DrawFilledQuad()
// ---------------------------------------------------------------------------
void Train::InitSkyVAO()
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


// ---------------------------------------------------------------------------
// ApplyConfig (JSON hot-reload)
// ---------------------------------------------------------------------------
void Train::ApplyConfig(const TrainObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    m_pulseSources.clear();
    m_obstacles.clear();

    // Pulse sources removed from Train map – skip JSON entries intentionally

    for (const auto& o : cfg.obstacles)
    {
        float cx = MIN_X + o.topLeft.x + o.size.x * 0.5f;
        float cy = MIN_Y + (HEIGHT - o.topLeft.y) - o.size.y * 0.5f;
        m_obstacles.push_back({ {cx, cy}, o.size });
    }
}


// ---------------------------------------------------------------------------
// IsPlayerHiding – crouching inside a train hiding spot blocks drone detection
// ---------------------------------------------------------------------------
bool Train::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    if (!isPlayerCrouching) return false;

    const float trainWorldLeft = MIN_X + m_trainOffset;
    for (const auto& spot : m_hidingSpots)
    {
        Math::Vec2 worldPos = { trainWorldLeft + spot.localCenter.x, MIN_Y + spot.localCenter.y };
        if (Collision::CheckAABB(playerPos, playerHitboxSize, worldPos, spot.size))
            return true;
    }
    return false;
}


// ---------------------------------------------------------------------------
// ResolveJumpThroughAABB – thin “pipe” tiers on Third_ThirdTrain: pass upward, land from above, crouch = fall through.
// ---------------------------------------------------------------------------
static bool ResolveJumpThroughAABB(Player& player, Math::Vec2& currentHbCenter,
                                   const Math::Vec2& playerHalfSize,
                                   const Math::Vec2& obsCenter, const Math::Vec2& obsSize,
                                   bool crouchHeld)
{
    Math::Vec2 obsHalf = obsSize * 0.5f;
    Math::Vec2 obsMin  = obsCenter - obsHalf;
    Math::Vec2 obsMax  = obsCenter + obsHalf;
    Math::Vec2 pMin    = currentHbCenter - playerHalfSize;
    Math::Vec2 pMax    = currentHbCenter + playerHalfSize;

    // Jump-through slab은 가로 구간에서만 고려.
    const bool horizOverlap = pMax.x > obsMin.x && pMin.x < obsMax.x;
    if (!horizOverlap)
        return false;

    if (crouchHeld)
        return false;

    const float platTop = obsMax.y;
    const float vy      = player.GetVelocity().y;
    const float feet    = currentHbCenter.y - playerHalfSize.y;

    // 이 발판보다 확실히 위층에 있으면 무시 (3층 위에서 2층/바닥 발판 간섭 차단).
    constexpr float kTierAboveSlab = 36.0f;
    if (feet > platTop + kTierAboveSlab)
        return false;

    // 빠르게 상승 중이면 무조건 통과.
    // (2층 파이프에서 점프 시작 프레임에 같은 파이프가 다시 스냅해 점프를 막는 문제 방지)
    constexpr float kRiseVy       = 52.0f;
    if (vy > kRiseVy)
        return false;

    // 핵심: 정확히 닿는 프레임(겹침 0)도 착지로 유지하도록 작은 상향 오차를 허용.
    constexpr float kSnapDownRange = 96.0f;
    constexpr float kSnapUpRange   = 8.0f;
    if (feet < platTop - kSnapDownRange || feet > platTop + kSnapUpRange)
        return false;

    Math::Vec2 newHbCenter = currentHbCenter;
    newHbCenter.y = platTop + playerHalfSize.y;
    Math::Vec2 shift = newHbCenter - currentHbCenter;
    player.SetPosition(player.GetPosition() + shift);
    currentHbCenter = newHbCenter;
    player.SetOnGround(true);
    return true;
}

// Maintain stable standing when the player is exactly touching a platform top.
// Without this, exact-contact frames may flip on/off ground and cause jitter.
static bool SnapToTopSupport(Player& player, Math::Vec2& currentHbCenter,
                             const Math::Vec2& playerHalfSize,
                             const Math::Vec2& obsCenter, const Math::Vec2& obsSize,
                             Train::TrainHitboxKind kind, bool crouchHeld)
{
    if (kind == Train::TrainHitboxKind::JumpThroughPipe && crouchHeld)
        return false;

    Math::Vec2 obsHalf = obsSize * 0.5f;
    Math::Vec2 obsMin  = obsCenter - obsHalf;
    Math::Vec2 obsMax  = obsCenter + obsHalf;

    const float pMinX = currentHbCenter.x - playerHalfSize.x;
    const float pMaxX = currentHbCenter.x + playerHalfSize.x;
    const bool horizOverlap = pMaxX > obsMin.x && pMinX < obsMax.x;
    if (!horizOverlap)
        return false;

    const float platTop = obsMax.y;
    const float feet    = currentHbCenter.y - playerHalfSize.y;
    const float vy      = player.GetVelocity().y;

    // Don't glue while strongly rising.
    if (vy > 60.0f)
        return false;

    const float aboveTol = (kind == Train::TrainHitboxKind::JumpThroughPipe) ? 10.0f : 8.0f;
    const float belowTol = (kind == Train::TrainHitboxKind::JumpThroughPipe) ? 72.0f : 24.0f;

    if (feet > platTop + aboveTol || feet < platTop - belowTol)
        return false;

    Math::Vec2 newHbCenter = currentHbCenter;
    newHbCenter.y = platTop + playerHalfSize.y;
    Math::Vec2 shift = newHbCenter - currentHbCenter;
    player.SetPosition(player.GetPosition() + shift);
    currentHbCenter = newHbCenter;
    player.SetOnGround(true);
    return true;
}


// ---------------------------------------------------------------------------
// ResolveAABB – identical algorithm to Underground so behaviour is consistent.
//   Uses actual penetration depth (min-of-max minus max-of-min) on each axis,
//   resolves along the axis with the SMALLER overlap.
//   Returns true if the player was pushed UP onto the surface (standing on top).
// ---------------------------------------------------------------------------
static bool IsThinHorizontalSlab(const Math::Vec2& obsSize)
{
    return obsSize.y <= 72.0f && obsSize.x >= obsSize.y * 3.0f;
}

static bool ResolveAABB(Player& player, Math::Vec2& currentHbCenter,
                         const Math::Vec2& playerHalfSize,
                         const Math::Vec2& obsCenter, const Math::Vec2& obsSize)
{
    if (!Collision::CheckAABB(currentHbCenter, playerHalfSize * 2.0f, obsCenter, obsSize))
        return false;

    Math::Vec2 obsHalf = obsSize * 0.5f;
    Math::Vec2 obsMin  = obsCenter - obsHalf;
    Math::Vec2 obsMax  = obsCenter + obsHalf;
    Math::Vec2 pMin    = currentHbCenter - playerHalfSize;
    Math::Vec2 pMax    = currentHbCenter + playerHalfSize;

    // 얇은 가로 발판: overlapY가 작을 때 overlapX < overlapY 로 가로 밀림이 나와 웅크리기 낙하 시 옆으로 튐 → 세로 착지만
    if (IsThinHorizontalSlab(obsSize))
    {
        const float platTop = obsMax.y;
        const float platBot = obsMin.y;
        const float feet    = currentHbCenter.y - playerHalfSize.y;
        const float vy      = player.GetVelocity().y;

        // 위층에 있을 때 아래쪽 얇은 발판(열차 바닥 등)과 몸만 겹치면 해석하지 않음
        constexpr float kTierAboveSlab = 36.0f;
        if (feet > platTop + kTierAboveSlab)
            return false;

        const bool horizOverlap = pMax.x > obsMin.x && pMin.x < obsMax.x;
        // 윗면 근처에서만 세로 스냅(공중에서 발판보다 훨씬 위에 있을 때는 건너뜀 → 가로 밀림 방지)
        if (horizOverlap && vy <= 160.0f && feet <= platTop + 56.0f && feet >= platTop - 88.0f)
        {
            Math::Vec2 newHbCenter = currentHbCenter;
            newHbCenter.y          = platTop + playerHalfSize.y;
            Math::Vec2 shift       = newHbCenter - currentHbCenter;
            player.SetPosition(player.GetPosition() + shift);
            currentHbCenter = newHbCenter;
            player.SetOnGround(true);
            return true;
        }
    }

    float overlapX = std::min(pMax.x, obsMax.x) - std::max(pMin.x, obsMin.x);
    float overlapY = std::min(pMax.y, obsMax.y) - std::max(pMin.y, obsMin.y);

    Math::Vec2 newHbCenter = currentHbCenter;
    bool landedOnTop = false;

    if (overlapX < overlapY)
    {
        // Horizontal push
        if (currentHbCenter.x < obsCenter.x)
            newHbCenter.x = obsMin.x - playerHalfSize.x;
        else
            newHbCenter.x = obsMax.x + playerHalfSize.x;
    }
    else
    {
        if (currentHbCenter.y < obsCenter.y)
        {
            // Player is below the obstacle – hit ceiling from underneath
            newHbCenter.y = obsMin.y - playerHalfSize.y;
            player.ResetVelocity();
        }
        else
        {
            // Player is above the obstacle – land on top surface
            newHbCenter.y = obsMax.y + playerHalfSize.y;
            // Only zero Y velocity (preserves X so the player can walk freely)
            player.SetOnGround(true);
            landedOnTop = true;
        }
    }

    Math::Vec2 shift = newHbCenter - currentHbCenter;
    player.SetPosition(player.GetPosition() + shift);
    currentHbCenter = newHbCenter;
    return landedOnTop;
}


// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
void Train::Update(double dt, Player& player, Math::Vec2 playerHitboxSize,
                   bool pulseAbsorbHeld, bool ignoreCarInjectPulseCost,
                   bool attackHeld, bool attackTriggered, Math::Vec2 mouseWorldPos)
{
    const float fdt = static_cast<float>(dt);
    m_encounterScriptTime += fdt;

    Math::Vec2 playerPos = player.GetPosition();

    // Only run when player is roughly within (or near) the train region.
    // Upper bound needs extra margin: on the 3F pipe (local top ≈ 1006)
    // the player centre can exceed MIN_Y + HEIGHT.
    const bool playerInSubway =
        playerPos.y >= MIN_Y && playerPos.y <= MIN_Y + HEIGHT + 400.0f &&
        playerPos.x >= MIN_X - 200.0f;

    if (!playerInSubway) return;

    {
        const float dTrain = m_trainOffset - m_prevTrainOffsetActors;
        ApplyTrainMotionToDronesAndRobots(dTrain);
        m_prevTrainOffsetActors = m_trainOffset;
    }

    // Valve clockwise drag interaction (Car5 top valve) -> tank spill VFX.
    {
        const Math::Vec2 valveWorld = { MIN_X + m_trainOffset + m_valveLocalCenter.x, MIN_Y + m_valveLocalCenter.y };
        const bool canHover = IsValveMouseHoverable(player.GetHitboxCenter(), playerHitboxSize, mouseWorldPos);

        if (!attackHeld)
            m_valveDragging = false;

        if (attackTriggered && canHover)
        {
            const Math::Vec2 d = mouseWorldPos - valveWorld;
            m_valvePrevMouseAngle = std::atan2(d.y, d.x);
            m_valveDragging = true;
        }

        if (m_valveDragging && attackHeld)
        {
            const Math::Vec2 d = mouseWorldPos - valveWorld;
            const float currA = std::atan2(d.y, d.x);
            const float delta = WrapAnglePi(currA - m_valvePrevMouseAngle);
            m_valvePrevMouseAngle = currA;

            // Clockwise in world space = negative angular delta.
            if (delta < 0.0f)
                m_valveOpenAccum = std::min(3.5f, m_valveOpenAccum + (-delta) * 1.2f);
        }
        else
            m_valveOpenAccum = std::max(0.0f, m_valveOpenAccum - fdt * 0.45f);

        m_valveOpenT = std::clamp(m_valveOpenAccum / 2.8f, 0.0f, 1.0f);
        const float pressureFollow = std::min(1.0f, fdt * 1.25f);
        m_valvePressureT += (m_valveOpenT - m_valvePressureT) * pressureFollow;
        if (m_valvePressureT > 0.01f)
            m_valveWaterAnimTime += fdt * (0.8f + m_valvePressureT * 2.7f);
    }
    UpdateValveWaterParticles(fdt);

    TryActivateCar5Encounter(player.GetHitboxCenter(), playerHitboxSize);

    UpdateCar2PurpleContainer(fdt, player, mouseWorldPos, attackTriggered, pulseAbsorbHeld, ignoreCarInjectPulseCost);
    UpdateCar3Siren(fdt, player, player.GetHitboxCenter(), playerHitboxSize, mouseWorldPos, attackHeld,
                    ignoreCarInjectPulseCost);

    // --- Drone / Robot: FourthTrain 스크립트 (Car5 발판 진입 전까지 정지) ---
    const bool isPlayerHiding = IsPlayerHiding(player.GetHitboxCenter(), playerHitboxSize, player.IsCrouching());
    player.SetHiding(isPlayerHiding);

    if (m_car5EncounterActive)
        UpdateTrainEncounterScript(fdt, player);
    else if (m_droneManager)
    {
        // 인카운터 전: Car5 드론은 저공 호버링 + 플레이어(히트박스) 방향으로 일정 속도 추적.
        auto& drones = m_droneManager->GetDrones();
        const float baseY = Train::MIN_Y + 95.f + 82.f;
        const float tl = MIN_X + m_trainOffset;
        const float c5L = tl + m_car1Width + m_car2Width + m_car3Width + m_car4Width;
        const float c5R = c5L + m_car5Width;
        const Math::Vec2 pTarget = player.GetHitboxCenter();
        constexpr float kPreApproach = 90.f;
        const float formationCenter = std::clamp(pTarget.x, c5L + 260.f, c5R - 260.f);
        const float formationSpacing = 160.f;
        for (size_t i = 0; i < drones.size(); ++i)
        {
            auto& d = drones[i];
            if (d.IsDead())
                continue;
            Math::Vec2 p = d.GetPosition();
            const float slot = static_cast<float>(i) - static_cast<float>(drones.size() - 1) * 0.5f;
            const float desiredX = std::clamp(formationCenter + slot * formationSpacing, c5L + 120.f, c5R - 120.f);
            p.x = std::clamp(p.x + std::clamp(desiredX - p.x, -kPreApproach * fdt, kPreApproach * fdt), c5L + 120.f, c5R - 120.f);
            p.y = baseY + std::sin(m_encounterScriptTime * 2.1f + static_cast<float>(i) * 0.78f) * 18.f;
            d.SetPosition(p);
            d.SetVelocity({ 0.f, 0.f });
            d.SetBaseSpeed(95.f);
        }
    }

    ApplyValveWaterDamageToEnemies(fdt);

    // Use hitbox center (matches Underground / other maps)
    Math::Vec2 currentHbCenter = player.GetHitboxCenter();
    const Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;

    // --- Config obstacle collision (static) ---
    for (const auto& obs : m_obstacles)
        ResolveAABB(player, currentHbCenter, playerHalfSize, obs.pos, obs.size);

    // --- Train hitbox collision ---
    const float trainWorldLeft = MIN_X + m_trainOffset;
    bool playerOnTrainSurface = false;
    bool playerOnJumpThroughSurface = false;

    const bool crouchHeld = player.IsCrouching();
    const bool crouchPressed = crouchHeld && !m_prevCrouchHeld;

    // ─── 파이프 드롭스루 (S 누르는 순간) ───
    // collision pass 전에 직접 검사: 파이프 top과 발 차이가 작으면 즉시 -100px nudge.
    // 이렇게 하면 SnapToTopSupport / ResolveJumpThroughAABB의 race 조건과 무관하게 확실히 드롭된다.
    if (m_pipeDropCooldown > 0.0f)
        m_pipeDropCooldown = std::max(0.0f, m_pipeDropCooldown - fdt);

    if (crouchPressed && m_pipeDropCooldown <= 0.0f)
    {
        const Math::Vec2 hc = player.GetHitboxCenter();
        const float feet    = hc.y - playerHalfSize.y;
        for (const auto& hb : m_trainHitboxes)
        {
            if (!hb.collision) continue;
            if (hb.kind != Train::TrainHitboxKind::JumpThroughPipe) continue;

            const float xMin = trainWorldLeft + hb.localCenter.x - hb.size.x * 0.5f;
            const float xMax = trainWorldLeft + hb.localCenter.x + hb.size.x * 0.5f;
            const float top  = MIN_Y          + hb.localCenter.y + hb.size.y * 0.5f;

            const bool xOverlap = (hc.x + playerHalfSize.x > xMin) && (hc.x - playerHalfSize.x < xMax);
            if (!xOverlap) continue;
            if (std::abs(feet - top) > 24.0f) continue; // 파이프 top 근처에 발이 있어야 함

            constexpr float kPipeDropNudge = 100.0f;
            player.SetPosition(player.GetPosition() + Math::Vec2{ 0.0f, -kPipeDropNudge });
            player.SetOnGround(false);
            m_pipeDropCooldown = 0.18f;
            m_crouchCarryLatched = true;
            currentHbCenter = player.GetHitboxCenter();
            break;
        }
    }

    for (const auto& hb : m_trainHitboxes)
    {
        if (!hb.collision)
            continue;
        if (IsCar2PurpleHitbox(hb) && m_car2HidePhase != Car2HidePhase::None)
            continue;

        Math::Vec2 hbWorld = {
            trainWorldLeft + hb.localCenter.x,
            MIN_Y          + hb.localCenter.y
        };

        bool landed = false;
        if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe)
        {
            if (m_pipeDropCooldown > 0.0f) continue; // 드롭 직후엔 파이프 다시 잡지 않음
            landed = ResolveJumpThroughAABB(player, currentHbCenter, playerHalfSize, hbWorld, hb.size, crouchHeld);
        }
        else
            landed = ResolveAABB(player, currentHbCenter, playerHalfSize, hbWorld, hb.size);

        if (landed)
        {
            playerOnTrainSurface = true;
            if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe)
                playerOnJumpThroughSurface = true;
        }
    }

    // Exact-touch stabilization pass:
    // if collision solver didn't mark landed this frame, keep support when feet are already on a top surface.
    if (!playerOnTrainSurface)
    {
        for (const auto& hb : m_trainHitboxes)
        {
            if (!hb.collision)
                continue;
            if (IsCar2PurpleHitbox(hb) && m_car2HidePhase != Car2HidePhase::None)
                continue;

            Math::Vec2 hbWorld = {
                trainWorldLeft + hb.localCenter.x,
                MIN_Y          + hb.localCenter.y
            };
            if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe && m_pipeDropCooldown > 0.0f)
                continue;
            if (SnapToTopSupport(player, currentHbCenter, playerHalfSize, hbWorld, hb.size, hb.kind, crouchHeld))
            {
                playerOnTrainSurface = true;
                if (hb.kind == Train::TrainHitboxKind::JumpThroughPipe)
                    playerOnJumpThroughSurface = true;
                break;
            }
        }
    }

    // 드롭은 위쪽에서 이미 처리됨. 여기서는 prev 상태만 갱신.
    m_prevCrouchHeld = crouchHeld;

    // Track whether player is on train this frame
    m_playerOnTrain = playerOnTrainSurface;

    // 발판에서 벗어났으면 지면 플래그 해제 — 다음 프레임부터 중력 적용 (걷다가 낭떠러지)
    if (!playerOnTrainSurface)
        player.SetOnGround(false);

    UpdateCarTransport(fdt, player, currentHbCenter, pulseAbsorbHeld, ignoreCarInjectPulseCost);

    // --- Entry countdown → automatic departure ---
    if (m_entryTimer >= 0.0f && m_trainState == TrainState::Stationary)
    {
        m_entryTimer += fdt;
        if (m_entryTimer >= TRAIN_DEPART_DELAY)
        {
            m_trainState     = TrainState::Moving;
            m_trainCurrentSpeed = 0.0f;
            m_departedMsgTimer = 2.0f; // show "Train is moving!" for 2 seconds
            m_trainStartSound.Play();
            m_trainRunLoopSound.Play();
            m_trainRunLoopSound.SetVolume(0.08f); // start quietly at departure
            Logger::Instance().Log(Logger::Severity::Info, "Train: Train is now moving!");
        }
    }

    // Tick the "departed" flash message
    if (m_departedMsgTimer > 0.0f)
        m_departedMsgTimer -= fdt;

    // --- Train movement ---
    if (m_trainState == TrainState::Moving)
    {
        m_trainCurrentSpeed = std::min(TRAIN_SPEED, m_trainCurrentSpeed + TRAIN_ACCEL * fdt);
        const float move = m_trainCurrentSpeed * fdt;
        m_trainOffset += move;

        // Running loop volume scales with speed ratio.
        // Quiet right after departure, louder as acceleration approaches max speed.
        const float speedRatio = (TRAIN_SPEED > 0.0f) ? (m_trainCurrentSpeed / TRAIN_SPEED) : 1.0f;
        const float runVol = 0.08f + speedRatio * 0.52f; // 0.08 -> 0.60
        m_trainRunLoopSound.SetVolume(runVol);

        const Math::Vec2 hc = player.GetHitboxCenter();
        const Math::Vec2 hh = playerHitboxSize * 0.5f;
        const float twLeft = MIN_X + m_trainOffset - move;
        const bool inCarryBand = HitboxInTrainCarryBand(twLeft, m_totalTrainWidth, hc, hh);

        if (inCarryBand)
            player.SetPosition(player.GetPosition() + Math::Vec2{ move, 0.0f });
    }

    // --- Left map boundary ---
    currentHbCenter = player.GetHitboxCenter();
    if (currentHbCenter.x - playerHalfSize.x < MIN_X)
        player.SetPosition({ player.GetPosition().x + (MIN_X - (currentHbCenter.x - playerHalfSize.x)), player.GetPosition().y });

    m_prevPlayerOnTrain = playerOnTrainSurface;
    m_prevOnJumpThroughSurface = playerOnJumpThroughSurface;
}


// ---------------------------------------------------------------------------
// StartEntryTimer – called once when the Train map becomes active
// ---------------------------------------------------------------------------
void Train::StartEntryTimer()
{
    if (m_entryTimer < 0.0f) // only start once
    {
        m_entryTimer  = 0.0f;
        m_trainState  = TrainState::Stationary;
        m_trainOffset = 0.0f;
        m_trainCurrentSpeed = 0.0f;
        m_prevPlayerOnTrain  = false;
        m_airborneFromTrain  = false;
        m_crouchCarryLatched = false;
        m_prevOnJumpThroughSurface = false;
        m_prevCrouchHeld = false;
        m_pipeDropCooldown = 0.0f;
        m_trainStartSound.Stop();
        m_trainRunLoopSound.Stop();
        m_valveDragging = false;
        m_valveOpenAccum = 0.0f;
        m_valveOpenT = 0.0f;
        m_valvePressureT = 0.0f;
        m_valveWaterAnimTime = 0.0f;
        m_valveParticleSpawnCarry = 0.0f;
        m_valveParticleCounter = 0;
        m_valveWaterParticles.clear();
        m_car5EncounterActive     = false;
        m_encounterScriptTime     = 0.f;
        m_prevTrainOffsetActors   = m_trainOffset;
        if (!m_droneWaterCd.empty())
            std::fill(m_droneWaterCd.begin(), m_droneWaterCd.end(), 0.f);
        if (!m_robotWaterCd.empty())
            std::fill(m_robotWaterCd.begin(), m_robotWaterCd.end(), 0.f);
        m_car2HidePhase        = Car2HidePhase::None;
        m_car2HideSeqTime      = 0.f;
        m_car2InsideCharge     = 0.f;
        m_car2EnterSavedValid  = false;
        m_car3SirenActive     = true;
        m_car3SirenInjectT    = 0.f;
        m_car3SirenSpawnTimer = 0.f;
        m_car3SirenPendingShutdown = false;
        if (m_sirenDroneManager)
            m_sirenDroneManager->ClearAllDrones();
        ResetCarTransportSlotsToInitialState();
        Logger::Instance().Log(Logger::Severity::Info,
            "Train: Entry timer started – train departs in %.1f s", TRAIN_DEPART_DELAY);
    }
}

void Train::RestartEntryTimer()
{
    // Drop guard so checkpoint respawn can restore true initial-train state.
    m_entryTimer = -1.0f;
    StartEntryTimer();
}


// ---------------------------------------------------------------------------
// GetDepartureAnnouncementText
// ---------------------------------------------------------------------------
std::string Train::GetDepartureAnnouncementText() const
{
    if (m_trainState == TrainState::Moving)
    {
        if (m_departedMsgTimer > 0.0f)
            return "The train is now moving!";
        return "";
    }

    if (m_entryTimer >= 0.0f && m_entryTimer < TRAIN_DEPART_DELAY)
    {
        int secsLeft = static_cast<int>(std::ceil(TRAIN_DEPART_DELAY - m_entryTimer));
        return "Train departs in " + std::to_string(secsLeft)
               + (secsLeft == 1 ? " second" : " seconds")
               + "  –  PLEASE BOARD!";
    }

    return "";
}


// ---------------------------------------------------------------------------
// DrawFilledQuad
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
// DrawBackground – sunset sky gradient with simple horizontal parallax
//   Called from GameplayState::DrawMainLayer before the texture-shader pass.
// ---------------------------------------------------------------------------
void Train::DrawBackground(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    // Only draw when the train area is potentially visible
    const float mapTop    = MIN_Y + HEIGHT;
    const float mapBottom = MIN_Y;
    if (cameraPos.y + HEIGHT < mapBottom || cameraPos.y - HEIGHT > mapTop) return;

    // Camera-visible interval (with safety margin) for dynamic repetition.
    // This keeps the draw count low while still preventing background seams.
    viewHalfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float drawMargin = 1400.0f;
    const float visibleLeft  = cameraPos.x - viewHalfW - drawMargin;
    const float visibleRight = cameraPos.x + viewHalfW + drawMargin;

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
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.90f }, { spanW, HEIGHT * 0.22f }, 0.13f, 0.05f, 0.19f, 1.0f);
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.75f }, { spanW, HEIGHT * 0.22f }, 0.22f, 0.08f, 0.20f, 0.95f);
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.60f }, { spanW, HEIGHT * 0.20f }, 0.38f, 0.11f, 0.18f, 0.90f);
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.47f }, { spanW, HEIGHT * 0.18f }, 0.58f, 0.17f, 0.14f, 0.88f);
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.36f }, { spanW, HEIGHT * 0.16f }, 0.80f, 0.28f, 0.11f, 0.85f);
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.25f }, { spanW, HEIGHT * 0.18f }, 0.53f, 0.18f, 0.10f, 0.70f);
    DrawFilledQuad(colorShader, { skyPx, MIN_Y + HEIGHT * 0.11f }, { spanW, HEIGHT * 0.22f }, 0.10f, 0.07f, 0.08f, 1.0f);

    // Sun + glow
    // Mild parallax for sun only (0.9x): keeps stability while adding depth.
    const float sunX = centerX + camDx * 0.9f + 320.0f;
    const float sunY = MIN_Y + HEIGHT * 0.37f;
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
        const float y1 = MIN_Y + HEIGHT * (0.78f - 0.02f * ((i + 30) % 4));
        const float y2 = MIN_Y + HEIGHT * (0.66f - 0.02f * ((i + 11) % 5));
        const float y3 = MIN_Y + HEIGHT * (0.56f - 0.015f * ((i + 7) % 6));

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
        DrawFilledQuad(colorShader, { x, MIN_Y + HEIGHT * 0.13f + h * 0.5f }, { w, h }, 0.10f, 0.06f, 0.09f, 0.95f);
    }

    // Near dark silhouette strip (foreground city/yard)
    const float nearStep = 210.0f;
    const int nearMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 250.0f) / nearStep));
    const int nearMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 250.0f) / nearStep));
    for (int i = nearMinI; i <= nearMaxI; ++i)
    {
        const float x = nearPx + i * 210.0f;
        const float h = 86.0f + static_cast<float>((i + 100) % 5) * 20.0f;
        DrawFilledQuad(colorShader, { x, MIN_Y + HEIGHT * 0.07f + h * 0.5f }, { 150.0f, h }, 0.07f, 0.05f, 0.06f, 1.0f);
    }

    // Poles / masts
    const float poleStep = 160.0f;
    const int poleMinI = static_cast<int>(std::floor((visibleLeft - nearPx - 120.0f) / poleStep));
    const int poleMaxI = static_cast<int>(std::ceil((visibleRight - nearPx + 120.0f) / poleStep));
    for (int i = poleMinI; i <= poleMaxI; ++i)
    {
        const float x = nearPx + i * 160.0f;
        DrawFilledQuad(colorShader, { x, MIN_Y + HEIGHT * 0.22f },
                       { 10.0f, 170.0f + static_cast<float>((i + 80) % 3) * 36.0f },
                       0.06f, 0.04f, 0.05f, 0.94f);
    }
}


// ---------------------------------------------------------------------------
// DrawCarTransportVFX — 시동된 차량 상단 펄스 라이트(플레이스홀더)
// ---------------------------------------------------------------------------
void Train::DrawCarTransportVFX(Shader& colorShader, Math::Vec2 cameraPos, float viewHalfW) const
{
    if (!m_skyVAO)
        return;

    float halfW = (viewHalfW > 300.0f) ? viewHalfW : 300.0f;
    const float margin  = 900.0f;
    const float visLeft = cameraPos.x - halfW - margin;
    const float visRight = cameraPos.x + halfW + margin;

    GL::Enable(GL_BLEND);
    GL::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float trainLeft = MIN_X + m_trainOffset;

    for (int i = 0; i < kCarTransportCount; ++i)
    {
        const auto& s = m_carTransportSlots[static_cast<size_t>(i)];
        if (!s.engineOn && s.engineGlowTimer <= 0.f)
            continue;

        const Math::Vec2 wc = { trainLeft + s.localCenter.x, MIN_Y + s.localCenter.y };
        if (wc.x + s.halfSize.x < visLeft || wc.x - s.halfSize.x > visRight)
            continue;

        const float flash = (s.engineGlowTimer > 0.f) ? std::min(1.f, s.engineGlowTimer * 2.f) : 0.f;
        const float baseA = s.engineOn ? 0.20f : 0.f;
        const float a     = std::min(1.f, baseA + flash * 0.55f);

        DrawFilledQuad(colorShader, wc, { s.halfSize.x * 1.65f, s.halfSize.y * 0.52f }, 0.22f, 0.82f, 1.0f, a);
        if (flash > 0.02f)
            DrawFilledQuad(colorShader, wc, { s.halfSize.x * 2.15f, s.halfSize.y * 0.82f }, 0.35f, 0.92f, 1.0f,
                           flash * 0.38f);
    }
}

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
void Train::Draw(Shader& shader, Math::Vec2 cameraPos, float viewHalfW) const
{
    // ── Rail tiles (static world position; draw only visible tile range) ──
    if (m_railTile && m_railTileW > 0.0f)
    {
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
// DrawDrones / DrawRadars / DrawGauges
// ---------------------------------------------------------------------------
void Train::DrawDrones(Shader& shader) const
{
    if (m_droneManager)
        m_droneManager->Draw(shader);
    if (m_sirenDroneManager)
        m_sirenDroneManager->Draw(shader);
}

void Train::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_droneManager)
        m_droneManager->DrawRadars(colorShader, debugRenderer);
    if (m_sirenDroneManager)
        m_sirenDroneManager->DrawRadars(colorShader, debugRenderer);
}

void Train::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    if (m_droneManager)
        m_droneManager->DrawGauges(colorShader, debugRenderer);
    if (m_sirenDroneManager)
        m_sirenDroneManager->DrawGauges(colorShader, debugRenderer);
    for (const auto& robot : m_robots)
    {
        if (!robot.IsDead())
            robot.DrawGauge(colorShader, debugRenderer);
    }
}


// ---------------------------------------------------------------------------
// DrawDebug
// ---------------------------------------------------------------------------
void Train::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    // Pulse source boxes (green)
    for (const auto& source : m_pulseSources)
        debugRenderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(), { 0.0f, 1.0f });

    // Config obstacle boxes (red)
    for (const auto& obs : m_obstacles)
        debugRenderer.DrawBox(colorShader, obs.pos, obs.size, { 1.0f, 0.0f });

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


// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void Train::Shutdown()
{
    m_trainStartSound.Stop();
    m_trainRunLoopSound.Stop();
    m_valveWaterParticles.clear();
    ShutdownValveWaterGpu();

    if (m_firstTrain)      m_firstTrain->Shutdown();
    if (m_secondTrain)     m_secondTrain->Shutdown();
    if (m_thirdTrain)      m_thirdTrain->Shutdown();
    if (m_thirdThirdTrain) m_thirdThirdTrain->Shutdown();
    if (m_fourthTrain)     m_fourthTrain->Shutdown();
    if (m_valveSprite)     m_valveSprite->Shutdown();
    if (m_railTile)        m_railTile->Shutdown();

    if (m_skyVAO) { GL::DeleteVertexArrays(1, &m_skyVAO); m_skyVAO = 0; }
    if (m_skyVBO) { GL::DeleteBuffers(1, &m_skyVBO);      m_skyVBO = 0; }

    for (auto& source : m_pulseSources) source.Shutdown();

    for (auto& robot : m_robots)
        robot.Shutdown();
    m_robots.clear();

    if (m_droneManager)
        m_droneManager->Shutdown();
    if (m_sirenDroneManager)
        m_sirenDroneManager->Shutdown();

    if (m_car2EnterPromptTex)
        m_car2EnterPromptTex->Shutdown();
    if (m_car2LeavePromptTex)
        m_car2LeavePromptTex->Shutdown();

    Logger::Instance().Log(Logger::Severity::Info, "Train Map Shutdown");
}


// ---------------------------------------------------------------------------
// Drone accessors
// ---------------------------------------------------------------------------
const std::vector<Drone>& Train::GetDrones() const { return m_droneManager->GetDrones(); }
std::vector<Drone>&       Train::GetDrones()        { return m_droneManager->GetDrones(); }

const std::vector<Drone>& Train::GetSirenDrones() const { return m_sirenDroneManager->GetDrones(); }

void Train::ClearAllDrones()
{
    if (m_droneManager)
        m_droneManager->ClearAllDrones();
    if (m_sirenDroneManager)
        m_sirenDroneManager->ClearAllDrones();
}
