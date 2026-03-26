//GameState.hpp

#pragma once

class GameState
{
public:
    virtual ~GameState() = default;

    virtual void Initialize() = 0;
    virtual void Update(double dt) = 0;
    virtual void Draw() = 0;
    virtual void Shutdown() = 0;

    // Return true if this state is rendered on top of the state below it
    // (e.g. fade-out transition revealing the next state underneath)
    virtual bool IsTransparent() const { return false; }

    // Return true to skip post-processing for this state.
    // The state below (background) is still rendered through post-processing;
    // this state is drawn directly to the default framebuffer afterwards.
    virtual bool BypassPostProcess() const { return false; }
};