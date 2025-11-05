#pragma once
#include "Vec2.hpp"

namespace Collision
{
    // AABB (Axis-Aligned Bounding Box) 충돌을 검사
    // 각 객체의 중심 위치와 전체 크기(가로, 세로)를 받음
    bool CheckAABB(Math::Vec2 centerA, Math::Vec2 sizeA, Math::Vec2 centerB, Math::Vec2 sizeB);
}