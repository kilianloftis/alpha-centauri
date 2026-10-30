#pragma once

#include "game/combat/CaptureRules.h"

#include "game/units/Unit.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;
class GameState;
class UnitPositionIndex;
struct CombatResult_t;

// Hands each unit to rCaptor as its CapturedUnit_t says, and clears its order.
void CaptureUnits(UnitPositionIndex& rPositions, Faction& rCaptor,
                  const std::vector<CapturedUnit_t>& rCaptured);

// Pays rAttacker's faction the PlanetPearlsBounty for a unit it defeated, then takes the unit when
// DefeatedUnitCapture allows and destroys it otherwise. Returns its id when it was captured; unset
// when it was destroyed.
std::optional<UnitId_t> RemoveDefeatedUnit(UnitPositionIndex& rPositions, Unit& rAttacker,
                                           Unit& rDefeated);

// After a fight in which rAttacker took units: ArtifactCaptureFriction for units taken on
// defeat, and a capture notice when the player is either side. rResult names at least one.
void ApplyCombatCaptureAftermath(GameState& rGameState, const Unit& rAttacker,
                                 const CombatResult_t& rResult);

} // namespace ac
