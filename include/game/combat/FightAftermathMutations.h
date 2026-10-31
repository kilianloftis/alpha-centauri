#pragma once

namespace ac
{

class GameState;
class Tile;
class Unit;
struct CombatResult_t;

// After a fight or duel, once collateral and promotion are done: allied-territory friction, the
// capture aftermath when rAttacker took units, then the last-defender loss of a base on
// rBattleTile, which may spend a native raider (rResult.bAttackerDestroyed). rAttacker may already
// be destroyed, under the caller's destruction deferral.
void ApplyFightConsequences(GameState& rGameState, Unit& rAttacker, const Tile& rBattleTile,
                            CombatResult_t& rResult);

} // namespace ac
