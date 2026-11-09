#pragma once

namespace ac
{

class BaseManager;
class GameState;
class Unit;

// Founds a base where rFounder stands and dispatches its on_found_effects. The founding
// order path spends a SingleUse founder after this returns.
BaseManager& FoundBase(GameState& rGameState, Unit& rFounder);

} // namespace ac
