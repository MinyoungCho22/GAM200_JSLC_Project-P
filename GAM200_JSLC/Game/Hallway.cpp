//Hallway.cpp

#include "Hallway.hpp"
#include "Background.hpp"
#include "Player.hpp"
#include "MapObjectTypes.hpp"
#include "../OpenGL/Shader.hpp"
#include "../Engine/Matrix.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/DebugRenderer.hpp"
#include <algorithm> 
#include <cmath>

constexpr float ROOM_WIDTH = 1920.0f;

// ---------------------------------------------------------------------------
// [Initialize]
// - 기능: Hallway 구역을 시작 상태로 초기화합니다. 배경 이미지, 철제 난간(railing) 텍스처 로드, 순찰 드론 2마리를 지정 위치에 스폰합니다.
// - 가이드라인: 드론의 초기 스폰 위치와 텍스처 경로는 정적으로 하드코딩되어 있습니다.
// ---------------------------------------------------------------------------
void Hallway::Initialize()
{
    m_background = std::make_unique<Background>();
    m_background->Initialize("Asset/Hallway.png");

    m_railing = std::make_unique<Background>();
    m_railing->Initialize("Asset/Railing.png");

    m_size = { WIDTH, HEIGHT };
    m_position = { ROOM_WIDTH + WIDTH / 2.0f, HEIGHT / 2.0f };

    m_droneManager = std::make_unique<DroneManager>();
    m_droneManager->SpawnDrone({ 2600.0f, 400.0f }, "Asset/Drone.png", DroneType::General);
    m_droneManager->SpawnDrone({ 5500.0f, 400.0f }, "Asset/Drone.png", DroneType::Detection);
}

// ---------------------------------------------------------------------------
// [ApplyConfig]
// - 기능: JSON 설정 데이터를 기반으로 펄스 소스 및 숨기기 영역(HidingSpots), 장애물 충돌 상자 좌표를 실시간 반영합니다.
// - 매개변수:
//   - cfg: Hallway 전용 맵 오브젝트 JSON 구조체
// - 가이드라인: 기존 생성된 펄스 소스와 은신처의 스프라이트 리소스를 메모리에서 안전하게 해제한 후 재배치합니다.
// ---------------------------------------------------------------------------
void Hallway::ApplyConfig(const HallwayObjectConfig& cfg)
{
    for (auto& source : m_pulseSources) source.Shutdown();
    m_pulseSources.clear();

    for (auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            spot.sprite->Shutdown();
            spot.sprite.reset();
        }
    }
    m_hidingSpots.clear();

    for (const auto& p : cfg.pulseSources)
    {
        float bottomY = HEIGHT - p.topLeft.y;
        Math::Vec2 center = { p.topLeft.x + p.size.x * 0.5f, bottomY + p.size.y * 0.5f };
        m_pulseSources.emplace_back();
        m_pulseSources.back().Initialize(center, p.size, 100.0f);
        if (!p.spritePath.empty()) m_pulseSources.back().InitializeSprite(p.spritePath.c_str());
    }

    for (const auto& h : cfg.hidingSpots)
    {
        float bottomY = HEIGHT - h.topLeft.y;
        Math::Vec2 center = { h.topLeft.x + h.size.x * 0.5f, bottomY + h.size.y * 0.5f };
        m_hidingSpots.emplace_back(HidingSpot{ center, h.size, nullptr });
        if (!h.spritePath.empty())
        {
            m_hidingSpots.back().sprite = std::make_unique<Background>();
            m_hidingSpots.back().sprite->Initialize(h.spritePath.c_str());
        }
    }

    float obsBottomY = HEIGHT - cfg.obstacle.topLeft.y;
    m_obstaclePos = { cfg.obstacle.topLeft.x + cfg.obstacle.size.x * 0.5f,
                      obsBottomY + cfg.obstacle.size.y * 0.5f };
    m_obstacleSize = cfg.obstacle.size;
}

// ---------------------------------------------------------------------------
// [Update]
// - 기능: 드론들의 행동 패턴을 업데이트하고 플레이어와 맵 내 정적 장애물 간의 AABB 충돌 해결을 처리합니다.
// - 매개변수:
//   - dt: 프레임 시간 델타
//   - playerCenter: 플레이어의 중심 좌표
//   - playerHitboxSize: 플레이어의 물리 충돌 상자 크기
//   - player: 플레이어 객체 레퍼런스 (위치 강제 스냅용)
//   - isPlayerHiding: 플레이어가 현재 은신 중인지 여부 (드론 감지 제어용)
// - 가이드라인: 장애물 충돌 시 겹침 영역이 더 얕은 축 방향으로 위치를 밀어내며, Y축 착지 시 수직 속도를 초기화하고 착지 상태로 설정합니다.
// ---------------------------------------------------------------------------
void Hallway::Update(double dt, Math::Vec2 playerCenter, Math::Vec2 playerHitboxSize, Player& player, bool isPlayerHiding)
{
    (void)playerCenter;
    m_droneManager->Update(dt, player, playerHitboxSize, isPlayerHiding, true, 1.f);

    Math::Vec2 playerPos = player.GetPosition();

    if (Collision::CheckAABB(playerPos, playerHitboxSize, m_obstaclePos, m_obstacleSize))
    {
        Math::Vec2 playerHalfSize = playerHitboxSize / 2.0f;
        Math::Vec2 obsHalfSize = m_obstacleSize / 2.0f;

        Math::Vec2 obsMin = m_obstaclePos - obsHalfSize;
        Math::Vec2 obsMax = m_obstaclePos + obsHalfSize;
        Math::Vec2 playerMin = playerPos - playerHalfSize;
        Math::Vec2 playerMax = playerPos + playerHalfSize;

        float overlapX = std::min(playerMax.x, obsMax.x) - std::max(playerMin.x, obsMin.x);
        float overlapY = std::min(playerMax.y, obsMax.y) - std::max(playerMin.y, obsMin.y);

        Math::Vec2 newPos = playerPos;

        if (overlapX < overlapY)
        {
            if (playerPos.x < m_obstaclePos.x)
                newPos.x = obsMin.x - playerHalfSize.x;
            else
                newPos.x = obsMax.x + playerHalfSize.x;
        }
        else
        {
            if (playerPos.y < m_obstaclePos.y)
                newPos.y = obsMin.y - playerHalfSize.y;
            else
                newPos.y = obsMax.y + playerHalfSize.y;

            player.ResetVelocity();
            player.SetOnGround(true);
        }
        player.SetPosition(newPos);
    }
}

// ---------------------------------------------------------------------------
// [Draw]
// - 기능: Hallway 배경 텍스처, 펄스 소스 스프라이트, 숨기기 박스 스프라이트를 차례로 렌더링합니다.
// - 매개변수:
//   - shader: 텍스처 렌더링용 스프라이트 셰이더 레퍼런스
// ---------------------------------------------------------------------------
void Hallway::Draw(Shader& shader)
{
    Math::Matrix model = Math::Matrix::CreateTranslation(m_position) * Math::Matrix::CreateScale(m_size);
    shader.setMat4("model", model);
    m_background->Draw(shader, model);

    for (const auto& source : m_pulseSources)
    {
        source.DrawSprite(shader);
    }

    for (const auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            shader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
            shader.setBool("flipX", false);
            Math::Matrix spotModel = Math::Matrix::CreateTranslation(spot.pos) * Math::Matrix::CreateScale(spot.size);
            spot.sprite->Draw(shader, spotModel);
        }
    }
}

// ---------------------------------------------------------------------------
// [DrawDrones]
// - 기능: 순찰 중인 드론들의 스프라이트를 드로우합니다.
// - 매개변수:
//   - shader: 스프라이트용 텍스처 셰이더 레퍼런스
// ---------------------------------------------------------------------------
void Hallway::DrawDrones(Shader& shader)
{
    m_droneManager->Draw(shader);
}

// ---------------------------------------------------------------------------
// [DrawForeground]
// - 기능: 화면 전면 레이어에 난간(Railing) 텍스처를 가로폭 만큼 바둑판식으로 반복하여 그립니다.
// - 매개변수:
//   - shader: 전경 텍스처용 스프라이트 셰이더 레퍼런스
// - 가이드라인: 플레이어 캐릭터 앞을 지나다니는 원경 감각 연출을 극대화하기 위해 캐릭터 드로우 이후 호출해야 합니다.
// ---------------------------------------------------------------------------
void Hallway::DrawForeground(Shader& shader)
{
    if (!m_railing) return;

    float railW = 240.0f;
    float railH = 207.0f;

    int railCount = static_cast<int>(std::ceil(WIDTH / railW));

    float startX = ROOM_WIDTH + (railW / 2.0f);
    float startY = railH / 2.0f;

    for (int i = 0; i < railCount; ++i)
    {
        Math::Vec2 railPos = { startX + (i * railW), startY };
        Math::Vec2 railSize = { railW, railH };

        Math::Matrix railModel = Math::Matrix::CreateTranslation(railPos) * Math::Matrix::CreateScale(railSize);

        shader.setMat4("model", railModel);
        m_railing->Draw(shader, railModel);
    }
}

// ---------------------------------------------------------------------------
// [DrawRadars]
// - 기능: 순찰 드론들의 레이더 범위 선(원형)을 렌더링합니다.
// ---------------------------------------------------------------------------
void Hallway::DrawRadars(const Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawRadars(colorShader, debugRenderer);
}

// ---------------------------------------------------------------------------
// [DrawGauges]
// - 기능: 드론들의 경고 수준 및 체력 게이지바를 렌더링합니다.
// ---------------------------------------------------------------------------
void Hallway::DrawGauges(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    m_droneManager->DrawGauges(colorShader, debugRenderer);
}

// ---------------------------------------------------------------------------
// [Shutdown]
// - 기능: Hallway 맵 리소스를 소멸시킵니다. 텍스처, 펄스 소스, 드론 관리자 객체의 소멸 작업을 일괄 처리합니다.
// ---------------------------------------------------------------------------
void Hallway::Shutdown()
{
    if (m_background)
    {
        m_background->Shutdown();
    }

    if (m_railing)
    {
        m_railing->Shutdown();
    }

    for (auto& source : m_pulseSources)
    {
        source.Shutdown();
    }

    for (auto& spot : m_hidingSpots)
    {
        if (spot.sprite)
        {
            spot.sprite->Shutdown();
            spot.sprite.reset();
        }
    }

    if (m_droneManager)
    {
        m_droneManager->Shutdown();
    }
}

// ---------------------------------------------------------------------------
// [ClearAllDrones]
// - 기능: 맵 내 모든 순찰 드론들을 제거합니다. (예: 맵 클리어 및 페이드 진행용)
// ---------------------------------------------------------------------------
void Hallway::ClearAllDrones()
{
    if (m_droneManager)
    {
        m_droneManager->ClearAllDrones();
    }
}

Math::Vec2 Hallway::GetPosition() const
{
    return m_position;
}

Math::Vec2 Hallway::GetSize() const
{
    return m_size;
}

const std::vector<Drone>& Hallway::GetDrones() const
{
    return m_droneManager->GetDrones();
}

std::vector<Drone>& Hallway::GetDrones()
{
    return m_droneManager->GetDrones();
}

const std::vector<PulseSource>& Hallway::GetPulseSources() const
{
    return m_pulseSources;
}

std::vector<PulseSource>& Hallway::GetPulseSources()
{
    return m_pulseSources;
}

// ---------------------------------------------------------------------------
// [RefillPulseSourcesAfterCheckpointRespawn]
// - 기능: 플레이어가 체크포인트에서 부활했을 때, 맵 내 모든 펄스 소스 충전소의 충전량을 최대치로 복구시킵니다.
// ---------------------------------------------------------------------------
void Hallway::RefillPulseSourcesAfterCheckpointRespawn()
{
    for (auto& s : m_pulseSources)
        s.RefillStock();
}

const std::vector<Hallway::HidingSpot>& Hallway::GetHidingSpots() const
{
    return m_hidingSpots;
}

// ---------------------------------------------------------------------------
// [IsPlayerHiding]
// - 기능: 플레이어가 Hallway에 배치된 엄폐물 박스 내부에서 웅크리고 있는지 검사하여 은신 판정을 결정합니다.
// - 매개변수:
//   - playerPos: 플레이어 현재 중심 좌표
//   - playerHitboxSize: 플레이어 히트박스 크기
//   - isPlayerCrouching: 앉아있는 상태 Boolean
// - 반환값: 은신 충족 시 true, 미충족 시 false
// ---------------------------------------------------------------------------
bool Hallway::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize, bool isPlayerCrouching) const
{
    if (!isPlayerCrouching)
    {
        return false;
    }

    for (const auto& spot : m_hidingSpots)
    {
        if (Collision::CheckAABB(playerPos, playerHitboxSize, spot.pos, spot.size))
        {
            return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// [DrawDebug]
// - 기능: 디버그 모드에서 엄폐 구역(녹색 바운더리)과 정적 장애물 충돌체(적색) 영역을 시각화합니다.
// ---------------------------------------------------------------------------
void Hallway::DrawDebug(Shader& colorShader, DebugRenderer& debugRenderer) const
{
    for (const auto& spot : m_hidingSpots)
    {
        debugRenderer.DrawBox(colorShader, spot.pos, spot.size, { 0.3f, 1.0f });
    }
    debugRenderer.DrawBox(colorShader, m_obstaclePos, m_obstacleSize, { 1.0f, 0.0f });
}

// ---------------------------------------------------------------------------
// [DrawSpriteOutlines]
// - 기능: 플레이어가 일정 거리 근처에 접근했을 때, 충전기 및 엄폐물 박스 외곽선에 그린-글로우 아웃라인을 렌더링합니다.
// - 매개변수:
//   - outlineShader: 아웃라인 전용 셰이더 레퍼런스
//   - playerPos: 플레이어 좌표
//   - proximityDist: 아웃라인을 활성화할 픽셀 단위 감지 임계 거리 (기본: 300.f)
// ---------------------------------------------------------------------------
void Hallway::DrawSpriteOutlines(Shader& outlineShader,
                                  Math::Vec2 playerPos, float proximityDist) const
{
    const float proxDistSq = proximityDist * proximityDist;

    for (const auto& source : m_pulseSources)
    {
        if (!source.HasSprite()) continue;
        float distSq = (playerPos - source.GetPosition()).LengthSq();
        if (distSq <= proxDistSq)
        {
            source.DrawOutline(outlineShader);
        }
    }

    for (const auto& spot : m_hidingSpots)
    {
        if (!spot.sprite) continue;
        float distSq = (playerPos - spot.pos).LengthSq();
        if (distSq <= proxDistSq)
        {
            int w = spot.sprite->GetWidth();
            int h = spot.sprite->GetHeight();
            if (w <= 0 || h <= 0) continue;

            outlineShader.setVec2("texelSize", 1.0f / w, 1.0f / h);
            outlineShader.setVec4("outlineColor", 0.15f, 1.0f, 0.35f, 1.0f);

            Math::Matrix model = Math::Matrix::CreateTranslation(spot.pos) * Math::Matrix::CreateScale(spot.size);
            spot.sprite->Draw(outlineShader, model);
        }
    }
}