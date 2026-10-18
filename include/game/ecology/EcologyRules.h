#pragma once

#include "game/base/BaseTypes.h"
#include "game/ecology/EcoDamageCalculator.h"

#include <vector>

namespace ac
{

class AtrocityLedger;
class BaseManager;
class Tile;

// What the eco damage formula reads: the base's tiles, minerals and effects, and the session
// ledgers.
EcoDamageInputs_t EcoDamageInputs(const BaseManager& rBase);

// Each counted atrocity adds its severity's virtual eco minerals.
int AtrocityEcoMinerals(const AtrocityLedger& rLedger, FactionId_t perpetrator);

// The chance of a fungal pop this turn: the eco damage, capped by eco_damage.json.
int FungalPopChance(const BaseManager& rBase);

// Workable tiles that are neither a base nor fungus.
std::vector<const Tile*> FungalPopTiles(const BaseManager& rBase);

} // namespace ac
