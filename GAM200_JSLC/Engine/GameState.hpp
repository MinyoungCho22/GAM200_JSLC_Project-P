#pragma once

class GameState
{
public:
    virtual ~GameState() = default;

    virtual void Initialize() = 0;
    virtual void Update(double dt) = 0;
    virtual void Draw() = 0;
    virtual void Shutdown() = 0;
};