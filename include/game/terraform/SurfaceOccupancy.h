#pragma once

#include "game/terraform/ElevationChange.h"

#include <vector>

namespace ac
{

class GameState;

// Building id that lets a land-domain base remain when its tile becomes water.
inline constexpr const char* k_PressureDomeBuildingId = "Pressure_Dome";

// Drops improvements whose domain no longer matches, floods a base that may not occupy water,
// and destroys units stranded on the wrong surface that no standing base shelters.
void ReconcileSurfaceFlips(GameState& rGameState, const std::vector<SurfaceFlip_t>& rFlips);

} // namespace ac
