#pragma once

#include "game/effects/EffectConfig.h"
#include "game/map/ImprovementConfigParser.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ac
{

class Tile;

// Which occupants may share a tile. Kept apart from the config parser: parsing turns JSON
// into ImprovementConfig_t, and this answers a game-rules question about two of them.

// Improvement `domain` land/sea vs a water (or land) surface — placement, Former start, and
// surface-flip clearance all use this.
bool DomainMatchesSurface(ImprovementDomain_t domain, bool bWater);
bool TileMatchesDomain(const Tile& rTile, ImprovementDomain_t domain);

// True when an Allow override names this unordered pair (one id may be empty = wildcard).
bool CoexistenceOverrideAllows(std::span<const CoexistenceOverrideEffect_t> overrides,
                               std::string_view a, std::string_view b);

// The one coexistence predicate: blocked by domain, or by an occupant either side excludes,
// or by a missing requiredOccupants entry. leavingIds do not block excludes; overrides then
// the tile's waivers can allow a denied pair.
bool OccupantsBlockPlacement(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                             std::span<const std::string> leavingIds,
                             std::span<const CoexistenceOverrideEffect_t> overrides);

// OccupantsBlockPlacement read the other way round, for call sites that ask permission
// rather than look for a blocker.
bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                         std::span<const std::string> leavingIds,
                         std::span<const CoexistenceOverrideEffect_t> overrides);

// Pairs rCandidate needs waived on this tile: stock-denied, but allowed by overrides.
// The placer records these on the tile so the waiver outlives the former that earned it.
std::vector<std::pair<std::string, std::string>> WaivedPairsFor(
    const Tile& rTile, const ImprovementConfig_t& rCandidate,
    std::span<const CoexistenceOverrideEffect_t> overrides);

// False for the Base: a base's tile marker leaves only with the base, never because another
// occupant excludes it.
bool IsRemovableByOccupancy(const ImprovementConfig_t& rOccupant);

// Removable improvements that cannot share the tile with rIncoming. Placing an improvement
// never removes terrain.
std::vector<std::string> ImprovementsDisplacedBy(
    const Tile& rTile, const ImprovementConfig_t& rIncoming,
    std::span<const CoexistenceOverrideEffect_t> overrides);

// rPlaced can go on the tile once the improvements it displaces are gone. Terrain and the Base
// are never displaced, so anything excluded that remains is a hard refusal.
bool ImprovementApplies(const Tile& rTile, const ImprovementConfig_t& rPlaced,
                        std::span<const CoexistenceOverrideEffect_t> overrides);

} // namespace ac
