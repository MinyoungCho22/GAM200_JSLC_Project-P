//Collision.hpp

#pragma once
#include "Vec2.hpp"

namespace Collision
{
    bool CheckAABB(Math::Vec2 centerA, Math::Vec2 sizeA, Math::Vec2 centerB, Math::Vec2 sizeB);
}