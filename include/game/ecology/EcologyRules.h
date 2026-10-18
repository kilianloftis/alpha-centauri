#pragma once

#include "game/base/BaseTypes.h"
#include "game/ecology/EcoDamageCalculator.h"

#include <vector>

namespace ac
{

class AtrocityLedger;
class BaseManager;
class NativeUnitRegistry;
class Tile;
class WorldMap;
struct NativeUnitConfig_t;

// What the eco damage formula reads: the base's tiles, minerals and effects, and the session
// ledgers.
EcoDamageInputs_t EcoDamageInputs(const BaseManager& rBase);

// Each counted atrocity adds its severity's virtual eco minerals.
int AtrocityEcoMinerals(const AtrocityLedger& rLedger, FactionId_t perpetrator);

// The chance of a fungal pop this turn: the eco damage, capped by eco_damage.json.
int FungalPopChance(const BaseManager& rBase);

// Workable tiles that are neither a base nor fungus.
std::vector<const Tile*> FungalPopTiles(const BaseManager& rBase);

struct BloomCandidates_t
{
    // Null when the origin is a base.
    const Tile* pOrigin = nullptr;
    // Radius-1 neighbours that are neither a base nor fungus, in tile order.
    std::vector<const Tile*> neighbours;
};

// A bloom takes the origin first and fills its remaining tiles from the neighbours.
BloomCandidates_t BloomCandidateTiles(const Tile& rOrigin, const WorldMap& rWorldMap);

// Native-life designs whose domain can stand on rTile.
std::vector<const NativeUnitConfig_t*> EligibleLifeforms(const Tile& rTile,
                                                         const NativeUnitRegistry& rNatives);

} // namespace ac
