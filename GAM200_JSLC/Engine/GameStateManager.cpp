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
    if (HasState())
    {
        states.back()->Draw();
    }
}

void GameStateManager::PushState(std::unique_ptr<GameState> state)
{
    states.push_back(std::move(state));
    states.back()->Initialize();
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