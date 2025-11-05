#include "Collision.hpp"

namespace Collision
{
    bool CheckAABB(Math::Vec2 centerA, Math::Vec2 sizeA, Math::Vec2 centerB, Math::Vec2 sizeB)
    {
        // A의 경계
        float minAx = centerA.x - sizeA.x / 2.0f;
        float maxAx = centerA.x + sizeA.x / 2.0f;
        float minAy = centerA.y - sizeA.y / 2.0f;
        float maxAy = centerA.y + sizeA.y / 2.0f;

        // B의 경계
        float minBx = centerB.x - sizeB.x / 2.0f;
        float maxBx = centerB.x + sizeB.x / 2.0f;
        float minBy = centerB.y - sizeB.y / 2.0f;
        float maxBy = centerB.y + sizeB.y / 2.0f;

        // 충돌 검사 (모든 축에서 겹쳐야 함)
        bool collisionX = maxAx > minBx && minAx < maxBx;
        bool collisionY = maxAy > minBy && minAy < maxBy;

        return collisionX && collisionY;
    }
}