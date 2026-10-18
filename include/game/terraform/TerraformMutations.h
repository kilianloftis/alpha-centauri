#pragma once

#include "game/terraform/TerraformRules.h"

#include <random>

namespace ac
{

class IUnitOrderWorld;
class Tile;
class TileEffectsContext;
class Unit;

// Place the project's improvement and fire its on_complete_effects against the Former's tile.
// An improvement refuses before it removes anything when terrain would still conflict, and
// records on the tile any pair the Former's coexistence override waived. Returns whether
// anything actually changed.
bool ApplyTerraformResult(Tile& rTile, const TerraformProject_t& rProject,
                          TileEffectsContext& rTileEffects, Unit& rFormer, std::mt19937& rRng,
                          IUnitOrderWorld* pWorld);

} // namespace ac
