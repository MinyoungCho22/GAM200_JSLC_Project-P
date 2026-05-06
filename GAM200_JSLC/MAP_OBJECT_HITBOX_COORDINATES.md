# Map Object Hitbox Coordinates

Top-Left 기준

## Room

<!-- 실내 맵: 블라인드, 펄스 소스, 방 경계 -->

| 오브젝트 | Raw Position (x, y) | Size (w, h) | 비고 |
|---|---:|---:|---|
| Room Bounds | (150.0, 930.0) | (1620.0, 780.0) | 플레이어 이동 경계 |
| Pulse Source 1 | (424.0, 360.0) | (51.0, 63.0) | `Room.cpp` top-left 입력값 |
| Pulse Source 2 | (692.0, 550.0) | (215.0, 180.0) | `Room.cpp` top-left 입력값 |
| Pulse Source 3 | (1414.0, 212.0) | (75.0, 33.0) | `Room.cpp` top-left 입력값 |
| Blind Interaction | (1105.0, 352.0) | (310.0, 300.0) | `Room.cpp` top-left 입력값 |

## Gameplay Doors (Room/Hallway transition)

<!-- GameplayState에서 생성되는 문 히트박스 -->

| 오브젝트 | Raw Position (x, y) | Size (w, h) | 비고 |
|---|---:|---:|---|
| Room -> Hallway Door | (1710.0, 440.0) | (50.0, 300.0) | `GameplayState.cpp` Door 초기화 값 |
| Hallway -> Rooftop Door | (7195.0, 400.0) | (300.0, 300.0) | `GameplayState.cpp` Door 초기화 값 |

## Hallway

<!-- 복도 맵: 은신처, 장애물, 펄스 소스 -->

| 오브젝트 | Raw Position (x, y) | Size (w, h) | 비고 |
|---|---:|---:|---|
| Hallway Map Bounds | (1920.0, 1080.0) | (5940.0, 1080.0) | 배경/영역 |
| Pulse Source 1 | (2454.0, 705.0) | (210.0, 312.0) | `Hallway.cpp` top-left 입력값 |
| Hiding Spot 1 | (2820.0, 923.0) | (381.0, 324.0) | `Hallway.cpp` top-left 입력값 |
| Hiding Spot 2 | (5620.0, 923.0) | (381.0, 324.0) | `Hallway.cpp` top-left 입력값 |
| Obstacle | (7489.0, 973.0) | (369.0, 645.0) | `Hallway.cpp` top-left 입력값 |

## Rooftop

<!-- 옥상 맵: 구멍 상호작용, 리프트, 펄스 소스 -->

| 오브젝트 | Raw Position (x, y) | Size (w, h) | 비고 |
|---|---:|---:|---|
| Rooftop Map Bounds | (8130.0, 1080.0) | (8400.0, 1080.0) | `Rooftop::MIN_X`, `Rooftop::MIN_Y` |
| Hole Interaction Box | (9951.0, 1506.0) | (785.0, 172.0) | `Rooftop.cpp` top-left 입력값 |
| Lift Platform (initial) | (13745.0, 1799.0) | (351.0, 345.0) | `Rooftop.cpp` top-left 입력값 |
| Pulse Source 1 | (12521.0, 1700.0) | (333.0, 240.0) | `Rooftop.cpp` top-left 입력값 |
| Pulse Source 2 | (12900.0, 1700.0) | (333.0, 240.0) | `Rooftop.cpp` top-left 입력값 |

## Underground

<!-- 지하 맵: 장애물, 램프, 펄스 소스, 적 스폰 -->                

| 오브젝트 | Raw Position (x, y) | Size (w, h) | 비고 |
|---|---:|---:|---|
| Underground Map Bounds | (16260.0, -2000.0) | (7920.0, 1080.0) | `Underground::MIN_X`, `Underground::MIN_Y` |
| Light overlay(s) | `underground.lights[]` in `map_objects.json` | 장애물과 동일 raw 좌표 | 배경 직후 그림, `size` 0×0이면 텍스처 원본 크기 |
| Obstacle 1 | (939.0, 834.0) | (561.0, 162.0) | `AddObstacle` 입력값 |
| Obstacle 2 | (1584.0, 627.0) | (288.0, 369.0) | `pannel1.png` 스프라이트 |
| Obstacle 3 | (3471.0, 834.0) | (561.0, 162.0) | `AddObstacle` 입력값 |
| Obstacle 4 | (4116.0, 627.0) | (288.0, 369.0) | `pannel2.png` 스프라이트 |
| Obstacle 5 | (5235.0, 834.0) | (561.0, 162.0) | `AddObstacle` 입력값 |
| Obstacle 6 | (6825.0, 627.0) | (296.0, 369.0) | `AddObstacle` 입력값 |
| Pulse Source 1 | (1949.0, 309.0) | (69.0, 255.0) | `Underground_Pulse.png`, 상호작용 히트박스 `hitbox_margin` 28 |
| Pulse Source 2 | (4485.0, 309.0) | (69.0, 255.0) | 동일 |
| Pulse Source 3a (디스코 좌) | (5847.0, 375.0) | 텍스처 원본 크기 | `disco_part1.png` (`size` 0×0 → 런타임에 픽셀 크기), `shared_pulse_group` 공유 잔량 |
| Pulse Source 3b (디스코 우) | part1 오른쪽 +123 + `layout_offset_x` | 텍스처 원본 크기 |
| Ramp 1 | (23400.0, -2010.0) | (780.0, 300.0) | `AddRamp(startX, bottomY, ...)` 입력값 |


### Underground Enemy Spawn Reference

<!-- 히트박스가 아닌 스폰 위치 참고 -->

| 오브젝트 | Position (x, y) | 비고 |
|---|---:|---|
| Robot Spawn 1 | (18239.0, -1685.0) | 로봇 스폰 |
| Robot Spawn 2 | (21060.0, -1685.0) | 로봇 스폰 |
| Drone Spawn 1 | (19423.0, -1450.0) | 드론 스폰 |
| Drone Spawn 2 | (22203.0, -1450.0) | 드론 스폰 |
| Drone Spawn 3 | (22787.0, -1450.0) | 드론 스폰 |

## Train

<!-- 지하철 맵: 현재 펄스 소스만 배치, 장애물은 코드상 미배치 -->

| 오브젝트 | Raw Position (x, y) | Size (w, h) | 비고 |
|---|---:|---:|---|
| Train Map Bounds | (24180.0, -3500.0) | (7920.0, 1080.0) | `Train::MIN_X`, `Train::MIN_Y` |
| Pulse Source 1 | (1400.0, 171.0) | (270.0, 258.0) | `AddPulseSource` 입력값 |
| Pulse Source 2 | (3759.0, 84.0) | (350.0, 114.0) | `AddPulseSource` 입력값 |

### Train Enemy Spawn Reference

<!-- 현재 코드 기준 로봇/드론 스폰 없음 -->

- 현재 `Train::Initialize()`에서 로봇/드론 스폰 코드는 없음.
