//Collision.cpp

#include "Collision.hpp"

namespace Collision
{
    bool CheckAABB(Math::Vec2 centerA, Math::Vec2 sizeA, Math::Vec2 centerB, Math::Vec2 sizeB)
    {
     
        float minAx = centerA.x - sizeA.x / 2.0f;
        float maxAx = centerA.x + sizeA.x / 2.0f;
        float minAy = centerA.y - sizeA.y / 2.0f;
        float maxAy = centerA.y + sizeA.y / 2.0f;

      
        float minBx = centerB.x - sizeB.x / 2.0f;
        float maxBx = centerB.x + sizeB.x / 2.0f;
        float minBy = centerB.y - sizeB.y / 2.0f;
        float maxBy = centerB.y + sizeB.y / 2.0f;

       
        bool collisionX = maxAx > minBx && minAx < maxBx;
        bool collisionY = maxAy > minBy && minAy < maxBy;

        return collisionX && collisionY;
    }

    bool CheckPointInAABB(Math::Vec2 point, Math::Vec2 boxCenter, Math::Vec2 boxSize)
    {
        float minX = boxCenter.x - boxSize.x / 2.0f;
        float maxX = boxCenter.x + boxSize.x / 2.0f;
        float minY = boxCenter.y - boxSize.y / 2.0f;
        float maxY = boxCenter.y + boxSize.y / 2.0f;

        return (point.x >= minX && point.x <= maxX && point.y >= minY && point.y <= maxY);
    }
}