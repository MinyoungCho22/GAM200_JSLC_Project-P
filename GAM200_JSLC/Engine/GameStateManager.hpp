//GameStateManager.hpp

#pragma once
#include <vector>
#include <memory>
#include "GameState.hpp"

class Engine;

class GameStateManager
{
public:
    GameStateManager(Engine& engine);
    void Update(double dt);
    void Draw();

    // Draw all states except the top (used when top state bypasses post-processing)
    void DrawBackground();
    // Draw only the top state (used when top state bypasses post-processing)
    void DrawTopState();
    // Returns true when the top state should skip the post-processing pass
    bool TopBypassesPostProcess() const;

    // After post-process present: draws layered foreground (e.g. player) on top of the final image.
    void DrawForegroundAfterPostProcess();

    void PushState(std::unique_ptr<GameState> state);
    void InsertBelow(std::unique_ptr<GameState> state); // insert under current top
    void PopState();
    void ChangeState(std::unique_ptr<GameState> state);
    bool HasState() const { return !states.empty(); }
    void Clear();
    Engine& GetEngine() { return engine; }

private:
    std::vector<std::unique_ptr<GameState>> states;
    Engine& engine;
};