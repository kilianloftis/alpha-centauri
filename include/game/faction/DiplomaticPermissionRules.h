#pragma once

#include "game/faction/DiplomacyConfig.h"
#include "game/faction/base/BaseTypes.h"

namespace ac
{

class BaseManager;
class Faction;
class GameState;
class Unit;

// The rules of the status a and b currently hold.
const DiplomaticStatusRules_t& StatusRulesFor(const GameState& rGameState, FactionId_t a,
                                              FactionId_t b);

// False for native life: it is never met and never holds a diplomatic status.
bool HasDiplomacy(const Faction& rFaction);

// partner's status with ally carries defensive_obligation, and partner is not already at
// Vendetta with aggressor. False when partner is ally or aggressor.
bool IsObligedToDefend(const GameState& rGameState, FactionId_t partner, FactionId_t ally,
                       FactionId_t aggressor);

// Whether rGuest's units may stand in territory owned by territoryOwner. Always true for
// unowned tiles and the guest's own territory.
bool MayEnterTerritoryOf(const Faction& rGuest, FactionId_t territoryOwner);

// Whether units of rA and faction b may stand on the same tile. Always true for one faction.
bool MayShareTiles(const Faction& rA, FactionId_t b);

// Whether rUnit may be repaired in rBase: its own faction's bases, or a faction whose status
// with it allows repair_at_bases. Per-turn healing does not exist yet; it should ask this.
bool MayRepairAt(const Unit& rUnit, const BaseManager& rBase);

} // namespace ac
