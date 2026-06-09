// Train_Init.cpp

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

// 드론 시각 크기를 전투용 비율(kCombatDroneVisualScale)로 축소함
void Train::ApplyCombatDroneVisualScale(Drone& d)
{
    const Math::Vec2 s = d.GetSize();
    d.SetSize({ s.x * kCombatDroneVisualScale, s.y * kCombatDroneVisualScale });
}

// 열차 맵 전체를 초기화함 — 텍스처·히트박스·드론·로봇·사운드·하늘 VAO를 세팅함
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
    m_pulseBoxSprite = std::make_unique<Background>();
    m_sirenSprite = std::make_unique<Background>();
    m_waterOpenerSprite = std::make_unique<Background>();
    m_tunnelPulseInjectorSprite = std::make_unique<Background>();
    m_secondTrain = std::make_unique<Background>();
    m_thirdTrain       = std::make_unique<Background>();
    m_car3InsideTrainA = std::make_unique<Background>();
    m_car3InsideTrainB = std::make_unique<Background>();
    m_tunnelCeilTex      = std::make_unique<Background>();
    m_tunnelFrontTex     = std::make_unique<Background>();
    m_tunnelBackTex      = std::make_unique<Background>();
    m_tunnelInsideTrain  = std::make_unique<Background>();
    m_tunnelObjectTex    = std::make_unique<Background>();
    m_secondTrainFrontTex = std::make_unique<Background>();
    m_thirdThirdTrain  = std::make_unique<Background>();
    m_fourthTrain      = std::make_unique<Background>();
    m_fifthTrain       = std::make_unique<Background>();
    m_valveSprite      = std::make_unique<Background>();

    m_firstTrain ->Initialize("Asset/Train/FirstTrain.png");
    m_pulseBoxSprite->Initialize("Asset/Train/PulseBox.png");
    m_sirenSprite->Initialize("Asset/Train/LED.png");
    m_waterOpenerSprite->Initialize("Asset/Train/WaterOpener.png");
    m_tunnelPulseInjectorSprite->Initialize("Asset/Train/Pulse_1.png");
    m_secondTrain->Initialize("Asset/Train/SecondTrain.png");
    m_thirdTrain ->Initialize("Asset/Train/ThirdTrain.png");
    m_car3InsideTrainA->Initialize("Asset/Train/SecondInside_1.png");
    m_car3InsideTrainB->Initialize("Asset/Train/SecondInside_2.png");
    m_tunnelCeilTex->Initialize("Asset/Train/Turnel_Upside.png");
    m_tunnelFrontTex->Initialize("Asset/Train/Turnel_Front.png");
    m_tunnelBackTex->Initialize("Asset/Train/Turnel_Back.png");
    m_tunnelInsideTrain->Initialize("Asset/Train/Turnel_Inside.png");
    m_tunnelObjectTex->Initialize("Asset/Train/Object.png");
    m_secondTrainFrontTex->Initialize("Asset/Train/Second_Front.png");
    if (m_tunnelInsideTrain->GetWidth() > 0)
        m_tunnelInsideWorldWidth = static_cast<float>(m_tunnelInsideTrain->GetWidth());
    m_tunnelInsideWorldLeft = MIN_X;
    {
        static const char* kCar3ExtPaths[kCar3ExtensionCount] = {
            "Asset/Train/SecondTrain_1.png",
            "Asset/Train/SecondTrain_2.png",
            "Asset/Train/SecondTrain_3.png",
        };
        for (int i = 0; i < kCar3ExtensionCount; ++i)
        {
            m_car3ExtensionTrains[static_cast<size_t>(i)] = std::make_unique<Background>();
            m_car3ExtensionTrains[static_cast<size_t>(i)]->Initialize(kCar3ExtPaths[i]);
            if (m_car3ExtensionTrains[static_cast<size_t>(i)]->GetWidth() > 0)
                m_car3ExtensionWidths[static_cast<size_t>(i)] =
                    static_cast<float>(m_car3ExtensionTrains[static_cast<size_t>(i)]->GetWidth());
            else
                m_car3ExtensionWidths[static_cast<size_t>(i)] = m_car2Width;
        }
    }
    m_thirdThirdTrain->Initialize("Asset/Train/Third_ThirdTrain.png");
    m_fourthTrain    ->Initialize("Asset/Train/FourthTrain.png");
    m_fifthTrain     ->Initialize("Asset/Train/Train_Head.png");
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
    if (m_fifthTrain->GetWidth()      > 0) m_car6Width = static_cast<float>(m_fifthTrain->GetWidth());
    m_totalTrainWidth = GetCar4LocalLeft() + m_car4Width + m_car5Width + m_car6Width;

    // Car5 valve anchor: centered on existing valve/pipe hitbox (c5, 894,351,317,162).
    // Keep local-space so it follows train offset automatically.
    {
        const float c5 = GetCar4LocalLeft() + m_car4Width;
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
    m_droneManager                 = std::make_unique<DroneManager>();
    m_sirenDroneManager            = std::make_unique<DroneManager>();
    m_carTransportDroneManager     = std::make_unique<DroneManager>();

    m_car2EnterPromptTex = std::make_unique<Background>();
    m_car2LeavePromptTex = std::make_unique<Background>();
    m_car2EnterPromptTex->Initialize("Asset/Train/Enter.png");
    m_car2LeavePromptTex->Initialize("Asset/Train/Leave.png");

    m_carTransportPulseLeftTex  = std::make_unique<Background>();
    m_carTransportPulseRightTex = std::make_unique<Background>();
    m_carTransportStartTex      = std::make_unique<Background>();
    m_carTransportPulseLeftTex->Initialize("Asset/Train/PulseLine_Left.png");
    m_carTransportPulseRightTex->Initialize("Asset/Train/PulseLine_Right.png");
    m_carTransportStartTex->Initialize("Asset/Train/Start.png");

    // --- Build train hitboxes from known pixel coordinates ---
    BuildTrainHitboxes();
    InitTunnelInsideProps();
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
    m_car3InsideViewActive = false;
    m_car3InsideTransitionActive = false;
    m_car3InsideTransitionTimer = 0.f;
    m_car3InsideTransitionTargetInside = false;
    m_car3InsideOnRoof                 = false;
    m_car3ExtensionStopTriggered       = false;
    m_trainDepartedOnce                = false;
    m_car3TunnelInsideViewActive       = false;
    m_car3TunnelInsideTransitionActive = false;
    m_car3TunnelInsideTransitionTimer  = 0.f;
    m_car3TunnelInsideTransitionTargetInside = false;
    m_car3TunnelBlend                  = 0.f;
    m_tunnelInsideInjectT              = 0.f;
    m_tunnelInsideInjectComplete       = false;
    m_tunnelInsideDepartStarted        = false;
    m_tunnelInsideTrainVisualX         = 0.f;
    m_tunnelInsideDepartTrainOffset0   = 0.f;
    m_tunnelInsideBoardTimer           = -1.f;
    m_playerOnTunnelBoardingFloor        = false;
    m_playerOnTunnelDepartWalkFloor      = false;

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

    // 드론: 1~4호차 각 2기 + 5호차 6기; Car4(카트랜스포트)는 덱 로봇 없음 · 1·2·5호차 덱 패트롤 + 5호차 광폭 2
    {
        for (auto& r : m_robots)
            r.Shutdown();
        m_robots.clear();
        if (m_droneManager)
            m_droneManager->ClearAllDrones();

        const float        c5   = GetCar4LocalLeft() + m_car4Width;
        constexpr float kLowY = Train::MIN_Y + 95.f + 88.f;
        const float     deckSurfaceY = Train::MIN_Y + kTrainFlatbedDeckTopLocalY;
        const float     highCarDroneHoverY =
            m_car3SirenHbValid ? (MIN_Y + m_car3SirenHb.localCenter.y + 55.f)
                                 : (deckSurfaceY + 380.f);

        for (int carIdx = 1; carIdx <= 5; ++carIdx)
        {
            const float l = GetTrainCarLocalLeftEdge(carIdx);
            float       w = m_car5Width;
            if (carIdx == 1)
                w = m_car1Width;
            else if (carIdx == 2)
                w = m_car2Width;
            else if (carIdx == 3)
                w = m_car3Width;
            else if (carIdx == 4)
                w = m_car4Width;

            if (carIdx == 5)
            {
                const float ySiren =
                    m_car3SirenHbValid ? (MIN_Y + m_car3SirenHb.localCenter.y + 55.f) : highCarDroneHoverY;
                constexpr int kCar5DroneCount = 6;
                // 물탱크 뒤(차량 앞쪽 / Car4 경계 쪽)와 앞(열차 끝 쪽)으로 나눠 배치, 속도·Y도 제각각
                for (int j = 0; j < kCar5DroneCount; ++j)
                {
                    const bool behindTank = j < 3;
                    const int  sub        = behindTank ? j : (j - 3);
                    const float span      = behindTank ? 0.34f : 0.34f;
                    const float baseT     = behindTank ? 0.10f : 0.56f;
                    const float t         = baseT + (static_cast<float>(sub) / 2.5f) * span;
                    const float xj =
                        MIN_X + l + w * t + static_cast<float>((j * 23 + 5) % 13 - 6) * 11.f;
                    const float yExtra = behindTank ? (-18.f - static_cast<float>(sub) * 14.f)
                                                    : (12.f + static_cast<float>(sub) * 16.f);
                    Drone& dj = m_droneManager->SpawnDrone({ xj, ySiren + yExtra }, kTrainDroneTexturePath, DroneType::General);
                    ScaleTrainCombatDrone(dj);
                    const float spd = behindTank ? (62.f + static_cast<float>(sub) * 19.f)
                                                 : (74.f + static_cast<float>(sub) * 21.f);
                    dj.SetBaseSpeed(spd);
                    dj.SetTrainCarSegment(5);
                }
                continue;
            }

            const float xA = MIN_X + l + w * 0.28f;
            const float xB = MIN_X + l + w * 0.72f;
            float       yA = kLowY + 12.f;
            float       yB = kLowY + 22.f;
            if (carIdx == 1 || carIdx == 2)
            {
                yA = highCarDroneHoverY + 8.f;
                yB = highCarDroneHoverY + 28.f;
            }
            else if (carIdx == 3 && m_car3SirenHbValid)
            {
                const float yAtSiren = MIN_Y + m_car3SirenHb.localCenter.y + 55.f;
                yA                 = yAtSiren + 8.f;
                yB                 = yAtSiren + 28.f;
            }
            Drone& da = m_droneManager->SpawnDrone({ xA, yA }, kTrainDroneTexturePath, DroneType::General);
            ScaleTrainCombatDrone(da);
            Drone& db = m_droneManager->SpawnDrone({ xB, yB }, kTrainDroneTexturePath, DroneType::General);
            ScaleTrainCombatDrone(db);
            if (carIdx == 1)
            {
                da.SetBaseSpeed(22.f);
                db.SetBaseSpeed(34.f);
            }
            else if (carIdx == 2)
            {
                da.SetBaseSpeed(18.f);
                db.SetBaseSpeed(30.f);
            }
            else
            {
                da.SetBaseSpeed(95.f);
                db.SetBaseSpeed(95.f);
            }
            da.SetTrainCarSegment(carIdx);
            db.SetTrainCarSegment(carIdx);
        }

        m_robots.emplace_back();
        {
            Robot&      rr        = m_robots.back();
            const float car1MidX  = MIN_X + m_car1Width * 0.52f;
            rr.Init({ car1MidX, deckSurfaceY + 260.f });
            ScaleTrainCombatRobot(rr);
            const float car1RobotY = deckSurfaceY + rr.GetSize().y * 0.5f;
            rr.SetPosition({ car1MidX, car1RobotY });
            rr.SetTrainCarSegment(1);
            rr.SetTrainDeckPatrol(true);
            rr.SetGroundLimitY(deckSurfaceY);
            rr.SetSpawnPosition(rr.GetPosition());
        }

        m_robots.emplace_back();
        {
            Robot&      rr        = m_robots.back();
            const float car2MidX  = MIN_X + m_car1Width + m_car2Width * 0.5f;
            rr.Init({ car2MidX, deckSurfaceY + 260.f });
            ScaleTrainCombatRobot(rr);
            const float car2RobotY = deckSurfaceY + rr.GetSize().y * 0.5f;
            rr.SetPosition({ car2MidX, car2RobotY });
            rr.SetTrainCarSegment(2);
            rr.SetTrainDeckPatrol(true);
            rr.SetGroundLimitY(deckSurfaceY);
            rr.SetSpawnPosition(rr.GetPosition());
        }

        for (int i = 0; i < 3; ++i)
        {
            m_robots.emplace_back();
            Robot&      rr       = m_robots.back();
            const float staggerY = static_cast<float>(i - 1) * 12.f;
            const float phaseX   = static_cast<float>(i - 1) * 16.f;
            const float cx =
                MIN_X + c5 + m_car5Width - 92.f - static_cast<float>(i) * 126.f + phaseX;
            rr.Init({ cx, deckSurfaceY + 260.f });
            ScaleTrainCombatRobot(rr);
            const float cy = deckSurfaceY + rr.GetSize().y * 0.5f + staggerY;
            rr.SetPosition({ cx, cy });
            rr.SetTrainCarSegment(5);
            rr.SetTrainDeckPatrol(true);
            rr.SetGroundLimitY(deckSurfaceY);
            rr.SetSpawnPosition(rr.GetPosition());
        }

        for (int b = 0; b < 2; ++b)
        {
            m_robots.emplace_back();
            Robot& rr = m_robots.back();
            const float bx  = MIN_X + c5 + m_car5Width * (0.38f + static_cast<float>(b) * 0.16f);
            const float bst = static_cast<float>(b) * 9.f;
            rr.Init({ bx, deckSurfaceY + 260.f });
            ScaleTrainCombatRobot(rr);
            rr.SetPosition({ bx, deckSurfaceY + rr.GetSize().y * 0.5f + bst });
            rr.SetTrainCarSegment(5);
            rr.SetTrainDeckPatrol(true);
            rr.SetGroundLimitY(deckSurfaceY);
            rr.SetSpawnPosition(rr.GetPosition());
            rr.ApplyTrainBerserkerProfile();
        }

        m_trainDeckRobotWasAirborne.assign(m_robots.size(), false);
        m_trainDeckRobotJumpPrepT.assign(m_robots.size(), 0.f);
        m_trainDeckRobotUsedLandingShake.assign(m_robots.size(), false);

        m_droneWaterCd.assign(m_droneManager->GetDrones().size(), 0.f);

        if (m_carTransportDroneManager)
        {
            m_carTransportDroneManager->ClearAllDrones();
            const float c4sum = GetCar4LocalLeft();
            for (int i = 0; i < kCarTransportHoverDroneCount; ++i)
            {
                const float lx = c4sum + kCarTransportDronePixels[i].x;
                const float ly = ASSUMED_IMG_HEIGHT - kCarTransportDronePixels[i].yTop;
                const float wx = MIN_X + lx;
                const float wy = MIN_Y + ly;
                Drone&      dd = m_carTransportDroneManager->SpawnDrone({ wx, wy }, kTrainDroneTexturePath, DroneType::General);
                ScaleTrainCombatDrone(dd);
                dd.SetBaseSpeed(0.f);
                dd.SetCarTransportPersistHover(true);
                dd.SetCarTransportBobPhase(static_cast<float>(i) * 0.71f);
                dd.SetTrainCarSegment(4);
            }
            m_carTransportDroneWaterCd.assign(m_carTransportDroneManager->GetDrones().size(), 0.f);
        }
    }

    m_prevTrainOffsetActors = m_trainOffset;
    m_car5EncounterActive    = false;
    m_car5ValveHintTimer     = 0.0f;
    m_trainCheatCarUnlock    = false;
    m_encounterScriptTime     = 0.f;
    m_trainDeckRobotWasAirborne.assign(m_robots.size(), false);
    m_trainDeckRobotJumpPrepT.assign(m_robots.size(), 0.f);
    m_trainDeckRobotUsedLandingShake.assign(m_robots.size(), false);
    m_pendingTrainCameraShakePx = 0.f;

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
// 각 열차 칸(Car1~5)의 충돌 히트박스·히딩 스팟·레일 슬랩을 픽셀 좌표 기반으로 구성함
void Train::BuildTrainHitboxes()
{
    m_trainHitboxes.clear();
    m_car5DeckHbValid = false;

    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 1  –  FirstTrain.png
    //    이미지 내 오브젝트 배치 (좌 → 우)
    //    [발판] - [빨강] - [회색 대형(상단)] - [황갈] - [은색(우측, 히딩)]
    // ════════════════════════════════════════════════════════════════════════
    const float c1 = 0.0f; // Car 1 시작 X (열차 맨 앞)

    // [Car1] 발판 (열차 바닥 플랫폼) ─ 플레이어가 올라서는 주요 발판
    //   위치: X=84  Y=804  크기: 2472 x 45
    //   ※ 세 열차 공통 – 높이(Y)나 두께(height) 조정 시 세 곳 모두 동일하게 변경
    m_trainHitboxes.push_back(MakeHitbox(c1,   84,  804, 2472,  45));

    // [Car1] 컨테이너 A  ─  좌측 빨간 컨테이너 (발판 위, 낮은 층)  FirstTrain.png
    //   (288, 518) 246×285
    m_trainHitboxes.push_back(MakeHitbox(c1, 288, 518, 246, 285));

    // [Car1] 컨테이너 B  ─  상단 회색 대형 컨테이너 (적재 상단)
    //   (534, 222) 549×297
    m_trainHitboxes.push_back(MakeHitbox(c1, 534, 222, 549, 297));

    // [Car1] 컨테이너 C  ─  중앙 황갈 컨테이너 (발판 위, 낮은 층)
    //   (1083, 519) 252×285
    m_trainHitboxes.push_back(MakeHitbox(c1, 1083, 519, 252, 285));

    // [Car1] 컨테이너 D  ─  우측 은색 컨테이너 (히딩박스 – 솔리드 히트박스 없음)
    //   (2005, 573) 525×237  →  m_hidingSpots에만 등록


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 2  –  SecondTrain.png (에셋 픽셀 기준)
    //    [발판] - [빨강] - [하단 회색] - [상단 보라(펄스박스)] - [우측 갈색]
    // ════════════════════════════════════════════════════════════════════════
    const float c2 = m_car1Width; // Car 2 시작 X (Car 1 끝 지점)

    m_trainHitboxes.push_back(MakeHitbox(c2, 84, 804, 2472, 45));

    // 빨강 (204, 525) 501×285
    m_trainHitboxes.push_back(MakeHitbox(c2, 204, 525, 501, 285));

    // 하단 어두운 회색 (1434, 519) 207×288
    m_trainHitboxes.push_back(MakeHitbox(c2, 1434, 519, 207, 288));

    // 보라 적재 · Car2 펄스 박스 (1653, 234) 603×285  →  m_car2PurpleHb
    {
        TrainHitbox purple = MakeHitbox(c2, 1653, 234, 603, 285);
        m_car2PurpleHb      = purple;
        m_car2PurpleHbValid = true;
        m_trainHitboxes.push_back(purple);
    }

    // [Car2] 보라 컨테이너 내부 바닥 발판 — 항상 활성, 컨테이너 안에서 플레이어가 서 있는 지지면
    // py=519 → 윗면 세계 Y = MIN_Y + 561 (dark gray 컨테이너 top과 동일한 높이)
    m_trainHitboxes.push_back(MakeHitbox(c2, 1653, 519, 603, 30));

    // 우측 갈색 (2268, 519) 183×285
    m_trainHitboxes.push_back(MakeHitbox(c2, 2268, 519, 183, 285));


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 3  –  ThirdTrain.png (에셋 픽셀 기준)
    //    [사이렌] [발판] - [좌 박스=히딩 전용] - [하단 중·우 회색] - [상단 황갈]
    // ════════════════════════════════════════════════════════════════════════
    const float c3 = m_car1Width + m_car2Width;

    // 사이렌 / 펄스 주입 (1644, 207) 126×162 — 충돌 없음
    {
        TrainHitbox siren = MakeHitbox(c3, 1644.f, 207.f, 126.f, 162.f, false);
        m_car3SirenHb      = siren;
        m_car3SirenHbValid = true;
        m_trainHitboxes.push_back(siren);
    }

    m_trainHitboxes.push_back(MakeHitbox(c3, 84, 804, 2472, 45));

    // I 좌측 밝은 회색 박스 (177, 567) 324×237  →  m_hidingSpots만 (솔리드 없음)

    // J 하단 중앙 어두운 회색 (1323, 516) 315×288
    m_trainHitboxes.push_back(MakeHitbox(c3, 1323, 516, 315, 288));

    // K 상단 황갈 (1638, 231) 456×285
    m_trainHitboxes.push_back(MakeHitbox(c3, 1638, 231, 456, 285));

    // L 하단 우측 어두운 회색 (2094, 516) 294×288
    m_trainHitboxes.push_back(MakeHitbox(c3, 2094, 516, 294, 288));


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 3½  –  SecondTrain_1~3 (ThirdTrain과 Third_Third 사이 연결 칸)
    // ════════════════════════════════════════════════════════════════════════
    float cExt = m_car1Width + m_car2Width + m_car3Width;
    for (int i = 0; i < kCar3ExtensionCount; ++i)
    {
        const float w = m_car3ExtensionWidths[static_cast<size_t>(i)];
        const float deckW = std::max(w - 168.0f, 400.0f);
        m_trainHitboxes.push_back(MakeHitbox(cExt, 84, 804, deckW, 45));
        if (i == 0)
        {
            // SecondTrain_1 좌측 인터랙션 박스(디버그 표시 + 마우스 클릭 전환)
            // 문 너비 260의 1/4 = 65px 오른쪽으로 이동 (420 → 485)
            m_car3ExtensionEnterHb = MakeHitbox(cExt, 485.f, 280.f, 260.f, 460.f, false);
            m_car3ExtensionEnterHbValid = true;
            // 문 오른쪽 경계: 문에서 한 발짝 오른쪽 (~900px)
            // localCenter는 cExt 기준이므로 cExt 더해서 저장
            m_car3DoorBarrierLocalX = cExt + 900.f;

            // SecondInside_1 — (372,351) 2049×399 내부, 좌우 경계 371 / 2421, 사다리 (1920,309) 150×444
            const float cIn = cExt;
            m_car3InsideFloorHb   = MakeHitbox(cIn, 372.f, 740.f, 2049.f, 45.f);
            m_car3InsideFloor2Hb  = MakeHitbox(cIn, m_car3ExtensionWidths[0] + 372.f, 740.f, 2049.f, 45.f);
            m_car3InsideFloor3Hb  = MakeHitbox(cIn,
                m_car3ExtensionWidths[0] + m_car3ExtensionWidths[1] + 372.f, 740.f, 2049.f, 45.f);
            m_car3InsideCeilingHb = MakeHitbox(cIn, 372.f, 351.f, 2049.f, 32.f);
            // 지붕: Inside_1~Inside_2 연속 (371 ~ car0폭+2421)
            const float roofW = m_car3ExtensionWidths[0] + kCar3InsideBoundRightPx - kCar3InsideBoundLeftPx;
            // 노란 지붕 바를 더 아래로 내려 열차 상단에 선 느낌으로 맞춘다.
            m_car3InsideRoofHb    = MakeHitbox(cIn, kCar3InsideBoundLeftPx, 180.f, roofW, 48.f);
            m_car3InsideLadderHb  = MakeHitbox(cIn, 1920.f, 309.f, 150.f, 444.f, false);
            // SecondInside_2 사다리(지붕에서 내부로 내려오기용).
            const float inside2LadderX = m_car3ExtensionWidths[0] + 1920.f;
            m_car3InsideLadder2Hb = MakeHitbox(cIn, inside2LadderX, 309.f, 150.f, 444.f, false);
            m_car3InsideLadderHbValid = true;
        }
        if (i == 2)
        {
            // SecondTrain_3 우측 터널 문(정지 후 좌클릭 → Turnel_Inside 전환)
            // 문 너비 260의 3/4 = 195px 오른쪽으로 이동 (w-950 → w-755)
            const float doorX = std::max(84.f, w - 755.f);
            m_car3TunnelEnterHb      = MakeHitbox(cExt, doorX, 120.f, 260.f, 680.f, false);
            m_car3TunnelEnterHbValid = true;
        }
        cExt += w;
    }

    // ════════════════════════════════════════════════════════════════════════
    // ▣  Car 4  –  Third_ThirdTrain.png
    //    발판만 충돌 — 자동차 박스는 디버그 표시 전용(collision false), 앞으로 걸어 통과
    // ════════════════════════════════════════════════════════════════════════
    const float c4 = cExt;

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
    // ▣  Car 6  –  Train_Head.png (물탱크 칸 오른쪽 추가 칸)
    // ════════════════════════════════════════════════════════════════════════
    const float c6 = c5 + m_car5Width;
    {
        const float deckW = std::max(m_car6Width - 168.0f, 400.0f);
        m_trainHitboxes.push_back(MakeHitbox(c6, 84, 804, deckW, 45));
    }


    // ════════════════════════════════════════════════════════════════════════
    // ▣  Hiding Spots  (기차와 함께 이동, 드론 탐지 차단)
    //    Car1 D / Car3 I  (Car4 자동차 윤곽은 비충돌 표시만)
    // ════════════════════════════════════════════════════════════════════════
    m_hidingSpots.clear();

    // [Car1] 컨테이너 D  ─  우측 은색 컨테이너  (2005, 573) 525×237
    {
        auto hs = MakeHitbox(c1, 2005, 573, 525, 237);
        HidingSpot spot{ hs.localCenter, hs.size, nullptr };
        spot.sprite = std::make_unique<Background>();
        spot.sprite->Initialize("Asset/Train/HidingBox.png");
        m_hidingSpots.push_back(std::move(spot));
    }

    // [Car3] 좌측 밝은 회색 박스 (177, 567) 324×237
    {
        auto hs = MakeHitbox(c3, 177, 567, 324, 237);
        HidingSpot spot{ hs.localCenter, hs.size, nullptr };
        spot.sprite = std::make_unique<Background>();
        spot.sprite->Initialize("Asset/Train/Third_HidingBox.png");
        m_hidingSpots.push_back(std::move(spot));
    }

    // rail.png — Draw와 동일한 타일 박스 안에서 실제 궤도 높이(kRailWalkSurfaceFractionOfTileH)에 발판 정렬.
    // localCenter: 맵 원점(MIN_X, MIN_Y) 기준 오프셋 (열차 히트박스와 동일).
    m_staticWorldHitboxes.clear();
    {
        const float     railSpanW = static_cast<float>(m_railTileCount) * m_railTileW;
        const float     tileH     = std::max(m_railTileH, 1.0f);
        const float     walkY     = std::clamp(
            tileH * kRailWalkSurfaceFractionOfTileH, 10.0f, std::max(12.0f, tileH - 4.0f));
        constexpr float kSlabH = 36.f;
        TrainHitbox     rail{};
        rail.localCenter = { railSpanW * 0.5f, walkY - kSlabH * 0.5f };
        rail.size        = { railSpanW, kSlabH };
        rail.collision   = true;
        rail.kind        = TrainHitboxKind::Solid;
        m_staticWorldHitboxes.push_back(rail);
    }
}


// ---------------------------------------------------------------------------
// Valve water — GPU instanced rendering (single batched draw)
// ---------------------------------------------------------------------------

// 하늘 그라데이션 렌더링에 쓰는 쿼드 VAO·VBO를 초기화함
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
// JSON 설정을 반영해 장애물 목록과 펄스 소스를 재구성함 (핫 리로드 대응)
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

// 열차 맵의 모든 텍스처·드론·로봇·사운드·GPU 리소스를 해제함
void Train::Shutdown()
{
    m_trainStartSound.Stop();
    m_trainRunLoopSound.Stop();
    m_valveWaterParticles.clear();
    ShutdownValveWaterGpu();

    if (m_firstTrain)      m_firstTrain->Shutdown();
    if (m_pulseBoxSprite)  { m_pulseBoxSprite->Shutdown(); m_pulseBoxSprite.reset(); }
    if (m_sirenSprite)     { m_sirenSprite->Shutdown(); m_sirenSprite.reset(); }
    if (m_waterOpenerSprite) { m_waterOpenerSprite->Shutdown(); m_waterOpenerSprite.reset(); }
    if (m_tunnelPulseInjectorSprite) { m_tunnelPulseInjectorSprite->Shutdown(); m_tunnelPulseInjectorSprite.reset(); }
    if (m_secondTrain)     m_secondTrain->Shutdown();
    if (m_thirdTrain)      m_thirdTrain->Shutdown();
    if (m_car3InsideTrainA) m_car3InsideTrainA->Shutdown();
    if (m_car3InsideTrainB) m_car3InsideTrainB->Shutdown();
    if (m_tunnelCeilTex)   m_tunnelCeilTex->Shutdown();
    if (m_tunnelFrontTex)    m_tunnelFrontTex->Shutdown();
    if (m_tunnelInsideTrain) m_tunnelInsideTrain->Shutdown();
    if (m_tunnelObjectTex)   m_tunnelObjectTex->Shutdown();
    if (m_secondTrainFrontTex) m_secondTrainFrontTex->Shutdown();
    for (auto& ext : m_car3ExtensionTrains)
    {
        if (ext)
            ext->Shutdown();
    }
    if (m_thirdThirdTrain) m_thirdThirdTrain->Shutdown();
    if (m_fourthTrain)     m_fourthTrain->Shutdown();
    if (m_fifthTrain)      m_fifthTrain->Shutdown();
    if (m_valveSprite)     m_valveSprite->Shutdown();
    if (m_railTile)        m_railTile->Shutdown();

    if (m_skyVAO) { GL::DeleteVertexArrays(1, &m_skyVAO); m_skyVAO = 0; }
    if (m_skyVBO) { GL::DeleteBuffers(1, &m_skyVBO);      m_skyVBO = 0; }

    for (auto& source : m_pulseSources) source.Shutdown();

    for (auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            spot.sprite->Shutdown();
            spot.sprite.reset();
        }
    }
    m_hidingSpots.clear();

    for (auto& robot : m_robots)
        robot.Shutdown();
    m_robots.clear();

    if (m_droneManager)
        m_droneManager->Shutdown();
    if (m_carTransportDroneManager)
        m_carTransportDroneManager->Shutdown();
    if (m_sirenDroneManager)
        m_sirenDroneManager->Shutdown();

    if (m_car2EnterPromptTex)
        m_car2EnterPromptTex->Shutdown();
    if (m_car2LeavePromptTex)
        m_car2LeavePromptTex->Shutdown();
    if (m_carTransportPulseLeftTex)
        m_carTransportPulseLeftTex->Shutdown();
    if (m_carTransportPulseRightTex)
        m_carTransportPulseRightTex->Shutdown();
    if (m_carTransportStartTex)
        m_carTransportStartTex->Shutdown();

    Logger::Instance().Log(Logger::Severity::Info, "Train Map Shutdown");
}