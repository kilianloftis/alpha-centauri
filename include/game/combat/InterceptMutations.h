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
// A hit destroys the attacker and returns the result with combat skipped; otherwise nullopt.
std::optional<CombatResult_t> TryInterceptAttack(GameState& rGameState,
                                                 Unit& rAttacker,
                                                 Unit& rDefender,
                                                 TileEffectsContext& rTileEffects,
                                                 std::mt19937& rRng);

} // namespace ac
