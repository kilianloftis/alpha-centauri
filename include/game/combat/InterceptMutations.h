#pragma once

#include "game/combat/CombatResolver.h"

#include <optional>
#include <random>

namespace ac
{

class GameState;
class TileEffectsContext;
class Unit;

// Before combat each ready CollectInterceptSources entry deploys and rolls; a miss may lose it.
// Returns the result when a combatant is lost: the attacker to a hit, the defender to its own miss.
std::optional<CombatResult_t> TryInterceptAttack(GameState& rGameState,
                                                 Unit& rAttacker,
                                                 Unit& rDefender,
                                                 TileEffectsContext& rTileEffects,
                                                 std::mt19937& rRng);

} // namespace ac
