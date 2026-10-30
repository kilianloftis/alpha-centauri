#pragma once

#include "game/terraform/TerraformRules.h"

#include <random>
#include <string>

namespace ac
{

class GameState;
class IUnitOrderWorld;
class Tile;
class TileEffectsContext;
class Unit;

// Starts a project CanStartTerraformProject allows: pays its energy up front, gives rFormer the
// TerraformOrder and spends its remaining moves.
void StartTerraform(GameState& rGameState, Unit& rFormer, const std::string& projectId);

// Places the project's improvement and fires its on_complete_effects, recording any waived
// pair. Refuses before removing anything if terrain would still conflict.
bool ApplyTerraformResult(Tile& rTile, const TerraformProject_t& rProject,
                          TileEffectsContext& rTileEffects, Unit& rFormer, std::mt19937& rRng,
                          IUnitOrderWorld& rWorld);

} // namespace ac
