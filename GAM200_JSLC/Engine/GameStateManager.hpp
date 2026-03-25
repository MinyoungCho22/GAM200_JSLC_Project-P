//GameStateManager.hpp

#pragma once
#include <vector>
#include <memory>

class GameState;
class Engine;

class GameStateManager
{
public:
    GameStateManager(Engine& engine);
    void Update(double dt);
    void DrawMainLayer();
    void DrawForegroundLayer();

    // Draw all states except the top (used when top state bypasses post-processing)
    void DrawBackgroundMainLayer();
    // Draw only the top state (used when top state bypasses post-processing)
    void DrawTopMainLayer();
    void DrawTopForegroundLayer();
    // Returns true when the top state should skip the post-processing pass
    bool TopBypassesPostProcess() const;

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