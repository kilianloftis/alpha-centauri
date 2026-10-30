#pragma once

namespace ac
{

class BaseManager;
class GameState;
class Unit;

// Founds a base where rFounder stands, for a unit CanFoundBase allows. A SingleUse founder is
// spent once the base has its starting minerals.
BaseManager& FoundBase(GameState& rGameState, Unit& rFounder);

// Credits starting minerals into rBase's stockpile. Founding only, after CreateBase while the
// founding unit still exists.
void ApplyStartingMinerals(BaseManager& rBase, const Unit* pFoundingUnit);

} // namespace ac
