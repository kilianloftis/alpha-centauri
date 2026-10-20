#pragma once

namespace ac
{

class GameState;
class WorldMap;

// The game state this thread is working on. Throws std::logic_error when there is none.
const GameState& CurrentGameState();
const GameState* FindCurrentGameState();

// Mutations and stages that take an explicit GameState& / WorldMap& while Rules read
// CurrentGameState() must agree. Throws std::logic_error when they do not.
void RequireCurrentGameState(const GameState& rGameState);
void RequireCurrentWorldMap(const WorldMap& rWorldMap);

// Makes a game state current on this thread for the scope's lifetime, then restores the
// previous one. Scopes nest.
class GameStateScope
{
public:
    explicit GameStateScope(const GameState& rGameState);
    ~GameStateScope();

    GameStateScope(const GameStateScope&) = delete;
    GameStateScope& operator=(const GameStateScope&) = delete;
    GameStateScope(GameStateScope&&) = delete;
    GameStateScope& operator=(GameStateScope&&) = delete;

private:
    const GameState* m_pInstalled = nullptr;
    const GameState* m_pPrevious = nullptr;
};

} // namespace ac
