// Room.cpp

#include "Room.hpp"
#include "../Engine/Collision.hpp"
#include "../Engine/ControlBindings.hpp"
#include "../Engine/DebugRenderer.hpp"
#include "../Engine/Engine.hpp"
#include "../Game/PulseCore.hpp"
#include "../OpenGL/Shader.hpp"
#include "MapObjectConfig.hpp"
#include "Player.hpp"


// Level design constants
constexpr float ROOM_WIDTH = 1620.0f;
constexpr float ROOM_HEIGHT = 780.0f;
constexpr float GROUND_LEVEL = 150.0f;
constexpr float GAME_WIDTH = 1920.0f;
constexpr float GAME_HEIGHT = 1080.0f;

// ---------------------------------------------------------------------------
// [Initialize]
// - 기능: Room 맵을 시작 상태로 초기화합니다. 배경 이미지 로드, 화면 가로폭 대비 경계 상자 계산, 초기 상태 초기화를 담당합니다.
// - 매개변수:
//   - engine: 엔진 코어 클래스 레퍼런스
//   - texturePath: 어두운 기본 배경 이미지 텍스처 경로
// - 가이드라인: 방의 크기 및 충돌 바운더리는 ROOM_WIDTH, ROOM_HEIGHT 상수를 기준으로 정적 화면 좌표계 상에 배치됩니다.
// ---------------------------------------------------------------------------
void Room::Initialize(Engine &engine, const char *texturePath) {
  // Load backgrounds for both lighting states
  m_background = std::make_unique<Background>();
  m_background->Initialize(texturePath);

  m_brightBackground = std::make_unique<Background>();
  m_brightBackground->Initialize("Asset/Room_Bright.png");

  // Define room boundaries relative to screen center
  float screenWidth = GAME_WIDTH;
  float minX = (screenWidth - ROOM_WIDTH) / 2.0f;
  float maxX = minX + ROOM_WIDTH;
  float minY = GROUND_LEVEL;
  float maxY = minY + ROOM_HEIGHT;

  m_boundaries.bottom_left = Math::Vec2(minX, minY);
  m_boundaries.top_right = Math::Vec2(maxX, maxY);
  m_roomSize = {ROOM_WIDTH, ROOM_HEIGHT};
  m_roomCenter = {minX + ROOM_WIDTH / 2.0f, minY + ROOM_HEIGHT / 2.0f};

  ApplyConfig(MapObjectConfig::Instance().GetData().room);

  m_isBright = false;
  m_playerInBlindArea = false;
}

// ---------------------------------------------------------------------------
// [ApplyConfig]
// - 기능: JSON 설정으로부터 Room 전용 맵 오브젝트(펄스 소스 충전기 및 블라인드 영역) 설정을 반영합니다.
// - 매개변수:
//   - cfg: Room 오브젝트 관련 JSON 파싱 데이터 구조체
// - 가이드라인: 리로드 기능 지원을 위해 기존 등록된 펄스 소스 텍스처/객체를 셧다운하고 새로 로드합니다.
// ---------------------------------------------------------------------------
void Room::ApplyConfig(const RoomObjectConfig &cfg) {
  for (auto &source : m_pulseSources)
    source.Shutdown();
  m_pulseSources.clear();

  for (const auto &p : cfg.pulseSources) {
    Math::Vec2 center = {p.topLeft.x + p.size.x * 0.5f,
                         p.topLeft.y - p.size.y * 0.5f};
    m_pulseSources.emplace_back();
    m_pulseSources.back().Initialize(center, p.size, 100.0f);
    if (!p.spritePath.empty())
      m_pulseSources.back().InitializeSprite(p.spritePath.c_str());
  }

  float blindBottomY = GAME_HEIGHT - cfg.blind.topLeft.y;
  m_blindPos = {cfg.blind.topLeft.x + cfg.blind.size.x * 0.5f,
                blindBottomY - cfg.blind.size.y * 0.5f};
  m_blindSize = cfg.blind.size;
}

// ---------------------------------------------------------------------------
// [Shutdown]
// - 기능: Room 맵이 해제될 때 텍스처 데이터와 자식 펄스 충전기들의 메모리를 명시적으로 소멸시킵니다.
// ---------------------------------------------------------------------------
void Room::Shutdown() {
  if (m_background)
    m_background->Shutdown();
  if (m_brightBackground)
    m_brightBackground->Shutdown();
  for (auto &source : m_pulseSources)
    source.Shutdown();
}

// ---------------------------------------------------------------------------
// [Update]
// - 기능: 플레이어와의 충돌(좌/우 외벽 및 천장) 처리와 블라인드 영역 상호작용(좌클릭 펄스 소모)을 매 프레임 업데이트합니다.
// - 매개변수:
//   - player: 플레이어 객체 레퍼런스 (위치 보정 및 펄스 조작용)
//   - dt: 프레임 시간 델타
//   - input: 입력 관리 클래스 레퍼런스
//   - mouseWorldPos: 현재 마우스 월드 위치
//   - controls: 컨트롤 바인딩 매핑 정보
// - 가이드라인:
//   - 플레이어가 블라인드 근처에 있을 때, 펄스 값이 `MIN_PULSE_TO_OPERATE_BLIND`(30.0f) 초과이고
//     소모 비용 `BLIND_TOGGLE_COST`(20.0f) 이상이어야 작동할 수 있습니다. 조건 불만족 시 거부 경고 플래그가 설정됩니다.
// ---------------------------------------------------------------------------
void Room::Update(Player &player, double dt, Input::Input &input,
                  Math::Vec2 mouseWorldPos, const ControlBindings &controls) {
  (void)dt;
  (void)mouseWorldPos;

  Math::Vec2 centerPos = player.GetPosition();
  Math::Vec2 halfSize = player.GetSize() * 0.5f;

  // Handle Wall Collision (Horizontal boundaries)
  if (centerPos.x - halfSize.x < m_boundaries.bottom_left.x) {
    player.SetPosition({m_boundaries.bottom_left.x + halfSize.x, centerPos.y});
  } else if (m_rightBoundaryActive &&
             centerPos.x + halfSize.x > m_boundaries.top_right.x) {
    player.SetPosition({m_boundaries.top_right.x - halfSize.x, centerPos.y});
  }

  // Handle Ceiling Collision (Vertical boundary)
  if (centerPos.y + halfSize.y > m_boundaries.top_right.y) {
    player.SetPosition({centerPos.x, m_boundaries.top_right.y - halfSize.y});
  }

  // Check for interaction with Blinds
  m_playerInBlindArea = Collision::CheckAABB(
      player.GetPosition(), player.GetHitboxSize(), m_blindPos, m_blindSize);

  // More forgiving interaction: if player is in blind area, left click works
  // without requiring a precise cursor-over-blind point test.
  if (m_playerInBlindArea &&
      controls.IsActionTriggered(ControlAction::Attack, input) && !m_isBright) {
    const float BLIND_TOGGLE_COST = 20.0f;
    const float MIN_PULSE_TO_OPERATE_BLIND = 30.0f;
    Pulse &pulse = player.GetPulseCore().getPulse();

    // Do not allow blind interaction when pulse is 30 or below.
    if (pulse.Value() > MIN_PULSE_TO_OPERATE_BLIND &&
        pulse.Value() >= BLIND_TOGGLE_COST) {
      pulse.spend(BLIND_TOGGLE_COST);
      m_isBright = true;
    } else {
      m_blindInteractDenied = true;
    }
  }
}

// ---------------------------------------------------------------------------
// [ConsumeBlindInteractDenied]
// - 기능: 블라인드 작동이 펄스 부족으로 거부되었는지 여부를 확인하고 해당 거부 플래그를 소비(초기화)합니다.
// - 반환값: 거부 여부 Boolean (소비하기 전 값)
// - 가이드라인: UI 경고 또는 배너 연출 시 1회성 판정을 위해 사용됩니다.
// ---------------------------------------------------------------------------
bool Room::ConsumeBlindInteractDenied() {
  const bool v = m_blindInteractDenied;
  m_blindInteractDenied = false;
  return v;
}

// ---------------------------------------------------------------------------
// [Draw]
// - 기능: Room의 배경과 하위 펄스 소스 스프라이트들을 렌더링합니다.
// - 매개변수:
//   - textureShader: 렌더링에 사용할 스프라이트 텍스처 셰이더 레퍼런스
// - 가이드라인: 블라인드가 열려 밝아진 상태(`m_isBright` == true)라면 `m_brightBackground`를 그리며, 그렇지 않으면 기본 배경을 그립니다.
// ---------------------------------------------------------------------------
void Room::Draw(Shader &textureShader) const {
  Math::Vec2 screenSize = {GAME_WIDTH, GAME_HEIGHT};
  Math::Vec2 screenCenter = screenSize * 0.5f;

  // Background fills the entire logical game area
  Math::Matrix bg_model = Math::Matrix::CreateTranslation(screenCenter) *
                          Math::Matrix::CreateScale(screenSize);
  textureShader.setMat4("model", bg_model);

  // Render appropriate background based on blind state
  if (m_isBright) {
    m_brightBackground->Draw(textureShader, bg_model);
  } else {
    m_background->Draw(textureShader, bg_model);
  }

  // 펄스 소스 오버레이 스프라이트 (Room_A, Room_B)
  // DrawSprite 전에 shader 상태를 명시적으로 설정해야 이전 draw pass의 값이 남지 않는다
  textureShader.use();
  textureShader.setFloat("alpha", 1.0f);
  textureShader.setVec4("spriteRect", 0.f, 0.f, 1.f, 1.f);
  textureShader.setBool("flipX", false);
  for (const auto &source : m_pulseSources) {
    source.DrawSprite(textureShader);
  }
}

// ---------------------------------------------------------------------------
// [DrawSpriteOutlines]
// - 기능: 플레이어가 근처에 있을 때 펄스 충전소들의 글로우 아웃라인을 그립니다.
// - 매개변수:
//   - outlineShader: 글로우 아웃라인 전용 셰이더 레퍼런스
//   - playerPos: 플레이어 현재 위치
//   - proximityDist: 아웃라인 활성화 임계 거리 (기본값: 300.0f)
// ---------------------------------------------------------------------------
void Room::DrawSpriteOutlines(Shader &outlineShader, Math::Vec2 playerPos,
                              float proximityDist) const {
  const float proxDistSq = proximityDist * proximityDist;
  for (const auto &source : m_pulseSources) {
    if (!source.HasSprite())
      continue;
    float distSq = (playerPos - source.GetPosition()).LengthSq();
    if (distSq <= proxDistSq)
      source.DrawOutline(outlineShader);
  }
}

// ---------------------------------------------------------------------------
// [DrawDebug]
// - 기능: 디버그 모드가 켜진 경우 물리 충돌 외벽(오렌지색), 펄스 소스 충격 지점(황색), 블라인드 상호작용 구역(백색/청색) 히트박스를 드로우합니다.
// - 매개변수:
//   - renderer: 디버그 사각형 드로잉을 수행할 렌더러 레퍼런스
//   - colorShader: 단색 렌더링용 셰이더 레퍼런스
//   - projection: 뷰-투영 행렬
//   - player: 플레이어 레퍼런스 (AABB 디버그 검사용)
// ---------------------------------------------------------------------------
void Room::DrawDebug(DebugRenderer &renderer, Shader &colorShader,
                     const Math::Matrix &projection,
                     const Player &player) const {
  (void)player;
  colorShader.use();
  colorShader.setMat4("projection", projection);

  // Draw level boundaries (Orange)
  renderer.DrawBox(colorShader, m_roomCenter, m_roomSize, {1.0f, 0.0f});

  // Draw PulseSource zones (Yellow/Amber)
  for (const auto &source : m_pulseSources) {
    renderer.DrawBox(colorShader, source.GetPosition(), source.GetSize(),
                     {1.0f, 0.5f});
  }

  // Draw Blind interaction zone (White if active, Light Blue otherwise)
  Math::Vec2 debugColor =
      m_playerInBlindArea ? Math::Vec2(1.0f, 1.0f) : Math::Vec2(0.5f, 1.0f);
  renderer.DrawBox(colorShader, m_blindPos, m_blindSize, debugColor);
}

// ---------------------------------------------------------------------------
// [IsPlayerHiding]
// - 기능: 플레이어가 블라인드 뒤에 정상적으로 은신하고 있는지 여부를 판정합니다.
// - 매개변수:
//   - playerPos: 플레이어 위치
//   - playerHitboxSize: 플레이어 히트박스 크기
//   - isPlayerCrouching: 플레이어가 앉은 상태(crouch)인지 여부
// - 반환값: 은신 중이면 true, 아니면 false
// - 가이드라인: 드론의 시야 검사 로직(Patrol AI)에서 호출하여 플레이어 감지를 방지하는 조건으로 사용합니다.
// ---------------------------------------------------------------------------
bool Room::IsPlayerHiding(Math::Vec2 playerPos, Math::Vec2 playerHitboxSize,
                          bool isPlayerCrouching) const {
  // Hiding is only possible if the player is actively crouching within the zone
  if (!isPlayerCrouching) {
    return false;
  }

  return Collision::CheckAABB(playerPos, playerHitboxSize, m_blindPos,
                              m_blindSize);
}