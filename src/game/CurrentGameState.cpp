#include "game/CurrentGameState.h"

#include <cassert>
#include <stdexcept>

namespace ac
{

namespace
{
thread_local const GameState* g_pCurrentGameState = nullptr;
}

const GameState& CurrentGameState()
{
    if (!g_pCurrentGameState)
    {
        throw std::logic_error("CurrentGameState: no game state is current on this thread");
    }
    return *g_pCurrentGameState;
}

const GameState* FindCurrentGameState()
{
    return g_pCurrentGameState;
}

GameStateScope::GameStateScope(const GameState& rGameState)
    : m_pInstalled(&rGameState)
    , m_pPrevious(g_pCurrentGameState)
{
    g_pCurrentGameState = m_pInstalled;
}

GameStateScope::~GameStateScope()
{
    // Out-of-order destruction would leave a destroyed game state current.
    assert(g_pCurrentGameState == m_pInstalled);
    g_pCurrentGameState = m_pPrevious;
}

} // namespace ac
