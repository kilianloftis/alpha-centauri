#pragma once

#include "game/terraform/ElevationChange.h"

#include <vector>

namespace ac
{

class GameState;

// Building id that lets a land-domain base remain when its tile becomes water.
inline constexpr const char* k_PressureDomeBuildingId = "Pressure_Dome";

// Drops improvements whose domain no longer matches and razes a base that may not occupy
// water.
void ReconcileSurfaceFlips(GameState& rGameState, const std::vector<SurfaceFlip_t>& rFlips);

} // namespace ac
