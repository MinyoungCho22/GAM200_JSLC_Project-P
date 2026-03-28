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

    const size_t n = states.size();
    GameState* top = states.back().get();

    // If the top state is transparent (e.g. a fade-out overlay),
    // first render the state below it so it shows through.
    if (n >= 2 && top->IsTransparent())
    {
        GameState* below = states[n - 2].get();
        if (below->UsesLayeredDraw())
        {
            below->DrawMainLayer();
            below->DrawForegroundLayer(false);
        }
        else
        {
            below->Draw();
        }
    }

    if (top->IsTransparent())
        top->Draw();
    else if (top->UsesLayeredDraw())
        top->DrawMainLayer();
    else
        top->Draw();
}

void GameStateManager::DrawForegroundAfterPostProcess()
{
    if (states.empty()) return;
    GameState* top = states.back().get();
    if (top->UsesLayeredDraw())
        top->DrawForegroundLayer(true);
}

void GameStateManager::DrawBackground()
{
    size_t n = states.size();
    if (n < 2) return;

    // Draw the state just below the top, respecting its own transparency.
    if (n >= 3 && states[n - 2]->IsTransparent())
        states[n - 3]->Draw();

    states[n - 2]->Draw();
}

void GameStateManager::DrawTopState()
{
    if (!states.empty())
        states.back()->Draw();
}

bool GameStateManager::TopBypassesPostProcess() const
{
    return !states.empty() && states.back()->BypassPostProcess();
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