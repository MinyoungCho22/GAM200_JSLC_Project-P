//GameStateManager.cpp

#include "GameStateManager.hpp"
#include "GameState.hpp"

GameStateManager::GameStateManager(Engine& engine_ref) : engine(engine_ref) {}

void GameStateManager::Update(double dt)
{
    if (HasState())
    {
        states.back()->Update(dt);
    }
}

void GameStateManager::Draw()
{
    if (states.empty()) return;

    // If the top state is transparent (e.g. a fade-out overlay),
    // first render the state below it so it shows through.
    if (states.size() >= 2 && states.back()->IsTransparent())
        states[states.size() - 2]->Draw();

    states.back()->Draw();
}

void GameStateManager::PushState(std::unique_ptr<GameState> state)
{
    states.push_back(std::move(state));
    states.back()->Initialize();
}

void GameStateManager::InsertBelow(std::unique_ptr<GameState> state)
{
    state->Initialize();
    if (states.empty())
    {
        states.push_back(std::move(state));
    }
    else
    {
        // Insert one position before the current top state
        states.insert(states.end() - 1, std::move(state));
    }
}

void GameStateManager::PopState()
{
    if (HasState())
    {
        states.back()->Shutdown();
        states.pop_back();
    }
}

void GameStateManager::ChangeState(std::unique_ptr<GameState> state)
{
    if (HasState())
    {
        PopState();
    }
    PushState(std::move(state));
}

void GameStateManager::Clear()
{
    while (HasState())
    {
        PopState();
    }
}