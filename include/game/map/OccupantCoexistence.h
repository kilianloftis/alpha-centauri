#pragma once

#include "game/effects/EffectConfig.h"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

struct ImprovementConfig_t;
class Tile;

// Which occupants may share a tile. Kept apart from the config parser: parsing turns JSON
// into ImprovementConfig_t, and this answers a game-rules question about two of them.

// The one coexistence predicate: blocked by domain, or by an occupant either side excludes.
// leavingIds do not block; overrides then the tile's waivers can allow a denied pair.
bool OccupantsBlockPlacement(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                             std::span<const std::string> leavingIds = {},
                             std::span<const CoexistenceOverrideEffect_t> overrides = {});

// OccupantsBlockPlacement read the other way round, for call sites that ask permission
// rather than look for a blocker.
bool CanBuildImprovement(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                         std::span<const std::string> leavingIds = {},
                         std::span<const CoexistenceOverrideEffect_t> overrides = {});

// Pairs rCandidate needs waived on this tile: stock-denied, but allowed by overrides.
// The placer records these on the tile so the waiver outlives the former that earned it.
std::vector<std::pair<std::string, std::string>> WaivedPairsFor(
    const Tile& rTile, const ImprovementConfig_t& rCandidate,
    std::span<const CoexistenceOverrideEffect_t> overrides);

// Improvements that cannot share the tile with rIncoming. Placing an improvement never
// removes terrain.
std::vector<std::string> ImprovementsDisplacedBy(
    const Tile& rTile, const ImprovementConfig_t& rIncoming,
    std::span<const CoexistenceOverrideEffect_t> overrides = {});

} // namespace ac
