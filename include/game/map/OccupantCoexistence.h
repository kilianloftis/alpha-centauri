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

// The one coexistence predicate. rCandidate is blocked by its domain, or by any occupant
// still on the tile that either side's `excludes` names. Modders declare the relationship
// once, on whichever side reads better, and every placement path honours it.
//
// leavingIds are occupants the caller is about to remove (or is re-evaluating in place), so
// they do not block. Re-checking an occupant that is already on the tile therefore names its
// own id, and a self-exclude does not evict the copy that is already there.
//
// overrides are the acting former's coexistence overrides whose conditions already passed.
// An override applies only when its cell differs from the stock exclude. The tile's own
// recorded waivers are folded in after them, so an override still wins a contested pair.
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

// Improvements already on the tile that cannot share it with rIncoming. Either side's
// excludes is enough, unless an override or a recorded waiver allows the pair. Terrain
// occupants are not listed: placing an improvement never removes terrain.
std::vector<std::string> ImprovementsDisplacedBy(
    const Tile& rTile, const ImprovementConfig_t& rIncoming,
    std::span<const CoexistenceOverrideEffect_t> overrides = {});

} // namespace ac
