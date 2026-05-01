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
#include <algorithm>
#include <cmath>
#include <string>


// ---------------------------------------------------------------------------
// Helper – pixel-space hitbox definition (image coords, Y from top of image)
//   Assumes each car image renders at its pixel size in world units.
//   localCenter.x = from train's initial left edge (MIN_X when offset=0)
//   localCenter.y = from map bottom (MIN_Y), Y going UP in world space
// ---------------------------------------------------------------------------
static constexpr float ASSUMED_IMG_HEIGHT = 1080.0f; // expected car image height in pixels

// Convert pixel (px,py) top-left + size to world local center (Y-flipped)
static Train::TrainHitbox MakeHitbox(float carOffset, float px, float py, float pw, float ph)
{
    float cx = carOffset + px + pw * 0.5f;
    float cy = ASSUMED_IMG_HEIGHT - py - ph * 0.5f; // flip Y: pixel-from-top → world-from-bottom
    return { {cx, cy}, {pw, ph} };
}


// ---------------------------------------------------------------------------
// Initialize
// ---------------------------------------------------------------------------
void Train::Initialize()
{
    Logger::Instance().Log(Logger::Severity::Info, "Train Map Initialize");

    // --- Train car images ---
    m_firstTrain  = std::make_unique<Background>();
    m_secondTrain = std::make_unique<Background>();
    m_thirdTrain  = std::make_unique<Background>();

    m_firstTrain ->Initialize("Asset/Train/FirstTrain.png");
    m_secondTrain->Initialize("Asset/Train/SecondTrain.png");
    m_thirdTrain ->Initialize("Asset/Train/ThirdTrain.png");

    // Use actual image widths as world widths (1 pixel = 1 world unit)
    if (m_firstTrain->GetWidth()  > 0) m_car1Width = static_cast<float>(m_firstTrain->GetWidth());
    if (m_secondTrain->GetWidth() > 0) m_car2Width = static_cast<float>(m_secondTrain->GetWidth());
    if (m_thirdTrain->GetWidth()  > 0) m_car3Width = static_cast<float>(m_thirdTrain->GetWidth());

    // --- Rail tile ---
    m_railTile = std::make_unique<Background>();
    m_railTile->Initialize("Asset/Train/rail.png");

    if (m_railTile->GetWidth() > 0)
        m_railTileW = static_cast<float>(m_railTile->GetWidth());
    if (m_railTile->GetHeight() > 0)
        m_railTileH = static_cast<float>(m_railTile->GetHeight());

    // How many rail tiles needed to cover train width (plus a few extra for safety)
    m_railTileCount = static_cast<int>(std::ceil(WIDTH / m_railTileW)) + 4;

    // --- Map extents (kept for legacy code / config) ---
    m_size     = { WIDTH, HEIGHT };
    m_position = { MIN_X + WIDTH * 0.5f, MIN_Y + HEIGHT * 0.5f };

    // --- Drone manager ---
    m_droneManager = std::make_unique<DroneManager>();

    // --- Build train hitboxes from known pixel coordinates ---
    BuildTrainHitboxes();

    // --- Sky gradient VAO ---
    InitSkyVAO();

    // --- Train state ---
    m_trainState      = TrainState::Stationary;
    m_trainOffset     = 0.0f;
    m_entryTimer      = -1.0f; // not started until StartEntryTimer() is called
    m_departedMsgTimer = 0.0f;
    m_playerOnTrain   = false;

    ApplyConfig(MapObjectConfig::Instance().GetData().train);

    Logger::Instance().Log(Logger::Severity::Info,
        "Train Map initialized – car widths: %.0f / %.0f / %.0f, rail tile: %.0f",
        m_car1Width, m_car2Width, m_car3Width, m_railTileW);
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
    m_trainHitboxes.push_back(MakeHitbox(c2, 1431,  234,  795, 285));

    // [Car2] 컨테이너 H  ─  우측 소형 황갈색 컨테이너 (발판 위, 낮은 층)
    //   위치: X=2324  Y=519  크기: 261 x 285
    m_trainHitboxes.push_back(MakeHitbox(c2, 2226,  519,  261, 285));


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 3  –  ThirdTrain.png
    //    [발판] - [컨테이너I(은색/흰)] - [컨테이너J(어두운 대형)] - [컨테이너K(황갈, 위)] - [컨테이너L(소형, 우측)]
    // ════════════════════════════════════════════════════════════════════════
    const float c3 = m_car1Width + m_car2Width; // Car 3 시작 X (Car 2 끝 지점)

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
    // ▣  Hiding Spots  (기차와 함께 이동, 드론 탐지 차단)
    //    Car1 우측 은색 대형 컨테이너(D)  /  Car3 좌측 은색 컨테이너(I)
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
// ResolveAABB – identical algorithm to Underground so behaviour is consistent.
//   Uses actual penetration depth (min-of-max minus max-of-min) on each axis,
//   resolves along the axis with the SMALLER overlap.
//   Returns true if the player was pushed UP onto the surface (standing on top).
// ---------------------------------------------------------------------------
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
void Train::Update(double dt, Player& player, Math::Vec2 playerHitboxSize)
{
    const float fdt = static_cast<float>(dt);

    Math::Vec2 playerPos = player.GetPosition();

    // Only run when player is roughly within (or near) the train region
    const bool playerInSubway =
        playerPos.y >= MIN_Y && playerPos.y <= MIN_Y + HEIGHT &&
        playerPos.x >= MIN_X - 200.0f;

    if (!playerInSubway) return;

    // --- Drone update (hiding spots block detection) ---
    const bool isPlayerHiding = IsPlayerHiding(player.GetHitboxCenter(), playerHitboxSize, player.IsCrouching());
    player.SetHiding(isPlayerHiding);
    m_droneManager->Update(dt, player, playerHitboxSize, isPlayerHiding);

    // Use hitbox center (matches Underground / other maps)
    Math::Vec2 currentHbCenter = player.GetHitboxCenter();
    const Math::Vec2 playerHalfSize = playerHitboxSize * 0.5f;

    // --- Config obstacle collision (static) ---
    for (const auto& obs : m_obstacles)
        ResolveAABB(player, currentHbCenter, playerHalfSize, obs.pos, obs.size);

    // --- Train hitbox collision ---
    const float trainWorldLeft = MIN_X + m_trainOffset;
    bool playerOnTrainSurface = false;

    for (const auto& hb : m_trainHitboxes)
    {
        Math::Vec2 hbWorld = {
            trainWorldLeft + hb.localCenter.x,
            MIN_Y          + hb.localCenter.y
        };
        if (ResolveAABB(player, currentHbCenter, playerHalfSize, hbWorld, hb.size))
            playerOnTrainSurface = true;
    }

    // Track whether player is on train this frame
    m_playerOnTrain = playerOnTrainSurface;

    // --- Entry countdown → automatic departure ---
    if (m_entryTimer >= 0.0f && m_trainState == TrainState::Stationary)
    {
        m_entryTimer += fdt;
        if (m_entryTimer >= TRAIN_DEPART_DELAY)
        {
            m_trainState     = TrainState::Moving;
            m_departedMsgTimer = 2.0f; // show "Train is moving!" for 2 seconds
            Logger::Instance().Log(Logger::Severity::Info, "Train: Train is now moving!");
        }
    }

    // Tick the "departed" flash message
    if (m_departedMsgTimer > 0.0f)
        m_departedMsgTimer -= fdt;

    // --- Train movement ---
    if (m_trainState == TrainState::Moving)
    {
        const float move = TRAIN_SPEED * fdt;
        m_trainOffset += move;

        // Move player with the train so it looks like they're riding it
        if (playerOnTrainSurface)
            player.SetPosition(player.GetPosition() + Math::Vec2{ move, 0.0f });
    }

    // --- Left map boundary ---
    currentHbCenter = player.GetHitboxCenter();
    if (currentHbCenter.x - playerHalfSize.x < MIN_X)
        player.SetPosition({ player.GetPosition().x + (MIN_X - (currentHbCenter.x - playerHalfSize.x)), player.GetPosition().y });
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
        Logger::Instance().Log(Logger::Severity::Info,
            "Train: Entry timer started – train departs in %.1f s", TRAIN_DEPART_DELAY);
    }
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
    const float centerX = MIN_X + WIDTH * 0.5f;
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
    m_droneManager->Draw(shader);
}

void Train::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

void Train::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);
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
        { MIN_X + WIDTH, MIN_Y + bndH * 0.5f }, { 10.0f, bndH }, { 1.0f, 1.0f });
}


// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void Train::Shutdown()
{
    if (m_firstTrain)  m_firstTrain->Shutdown();
    if (m_secondTrain) m_secondTrain->Shutdown();
    if (m_thirdTrain)  m_thirdTrain->Shutdown();
    if (m_railTile)    m_railTile->Shutdown();

    if (m_skyVAO) { GL::DeleteVertexArrays(1, &m_skyVAO); m_skyVAO = 0; }
    if (m_skyVBO) { GL::DeleteBuffers(1, &m_skyVBO);      m_skyVBO = 0; }

    for (auto& source : m_pulseSources) source.Shutdown();

    if (m_droneManager) m_droneManager->Shutdown();

    Logger::Instance().Log(Logger::Severity::Info, "Train Map Shutdown");
}


// ---------------------------------------------------------------------------
// Drone accessors
// ---------------------------------------------------------------------------
const std::vector<Drone>& Train::GetDrones() const { return m_droneManager->GetDrones(); }
std::vector<Drone>&       Train::GetDrones()        { return m_droneManager->GetDrones(); }
void                      Train::ClearAllDrones()   { m_droneManager->ClearAllDrones(); }
