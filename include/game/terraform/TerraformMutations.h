#pragma once

#include "game/terraform/TerraformRules.h"

#include <random>

namespace ac
{

class IUnitOrderWorld;
class Tile;
class TileEffectsContext;
class Unit;

// Places the project's improvement and fires its on_complete_effects, recording any waived
// pair. Refuses before removing anything if terrain would still conflict.
bool ApplyTerraformResult(Tile& rTile, const TerraformProject_t& rProject,
                          TileEffectsContext& rTileEffects, Unit& rFormer, std::mt19937& rRng,
                          IUnitOrderWorld* pWorld);

} // namespace ac
