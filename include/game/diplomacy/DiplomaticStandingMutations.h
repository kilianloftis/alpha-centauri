#pragma once

#include "game/base/BaseTypes.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/map/Tile.h"

#include <span>

namespace ac
{

class Faction;
class GameState;

// holder resents against by delta, clamped to friction_max.
void ApplyFriction(GameState& rGameState, FactionId_t holder, FactionId_t against, int delta);

// faction's blemishes after change, clamped to blemishes_max.
void ApplyBlemishes(GameState& rGameState, FactionId_t faction, BlemishChange_t change);

// AlliedTerritoryOwner's faction gains allied_territory_battle_friction toward attacker.
void ApplyAlliedTerritoryFriction(GameState& rGameState, FactionId_t attacker,
                                  FactionId_t defender, const Tile& rBattleTile);

// RainfallGrievant's faction gains terraform_rainfall_friction toward rTerraformer.
void ApplyRainfallGrievance(GameState& rGameState, const Faction& rTerraformer,
                            std::span<const MoistureChange_t> changes);

} // namespace ac
