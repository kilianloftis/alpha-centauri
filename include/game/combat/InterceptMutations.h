#pragma once

#include "game/combat/CombatResolver.h"

#include <optional>
#include <random>

namespace ac
{

class GameState;
class TileEffectsContext;
class Unit;

// Before CombatResolver::Resolve: each ready source from CollectInterceptSources deploys and
// rolls its chance; a miss may lose the source. A hit destroys the attacker and returns a
// CombatResult_t with combat skipped. Returns nullopt when no intercept fires.
std::optional<CombatResult_t> TryInterceptAttack(GameState& rGameState,
                                                 Unit& rAttacker,
                                                 Unit& rDefender,
                                                 TileEffectsContext& rTileEffects,
                                                 std::mt19937& rRng);

} // namespace ac
