#pragma once

#include "game/units/CombatResolver.h"

#include <optional>
#include <random>

namespace ac
{

class GameState;
class TileEffectsContext;
class Unit;

// Before CombatResolver::Resolve: roll ready InterceptAttempt sources on the defender side.
// Sources are gathered from four lanes — the defender faction's pool, the base on the
// defender's tile, the defender's own design, and the tile's area effects.
// On success, destroys the attacker and returns a CombatResult_t with combat skipped.
// Returns nullopt when no intercept fires (including when all attempts miss).
std::optional<CombatResult_t> TryInterceptAttack(GameState& rGameState,
                                                 Unit& rAttacker,
                                                 Unit& rDefender,
                                                 TileEffectsContext& rTileEffects,
                                                 std::mt19937& rRng);

} // namespace ac
