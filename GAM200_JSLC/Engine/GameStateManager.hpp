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
    void Draw();
    void PushState(std::unique_ptr<GameState> state);
    void PopState();
    void ChangeState(std::unique_ptr<GameState> state);
    bool HasState() const { return !states.empty(); }
    void Clear();
    Engine& GetEngine() { return engine; }

private:
    std::vector<std::unique_ptr<GameState>> states;
    Engine& engine;
};