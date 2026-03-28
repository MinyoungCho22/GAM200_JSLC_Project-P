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

    // When true, Draw() is not used for the top state during the FBO pass; the engine calls
    // DrawMainLayer() into the scene FBO, runs post-process, then DrawForegroundLayer(true)
    // on the default framebuffer (player/UI stay un-darkened by exposure).
    virtual bool UsesLayeredDraw() const { return false; }
    virtual void DrawMainLayer() {}
    // compositeToScreen: true = default FB + letterbox viewport after present; false = current
    // FBO (same virtual resolution) when compositing under a transparent overlay.
    virtual void DrawForegroundLayer(bool compositeToScreen = true) { (void)compositeToScreen; }
};