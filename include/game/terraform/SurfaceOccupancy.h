#pragma once

#include "game/terraform/ElevationChange.h"

#include <vector>

namespace ac
{

class IUnitOrderWorld;
class TileEffectsContext;

// Building id that lets a land-domain base remain when its tile becomes water.
inline constexpr const char* k_PressureDomeBuildingId = "Pressure_Dome";

// Drops improvements whose domain no longer matches and razes a base that may not occupy
// water. With a null pWorld, a base that must be razed throws.
void ReconcileSurfaceFlips(TileEffectsContext& rTileEffects, IUnitOrderWorld* pWorld,
                           const std::vector<SurfaceFlip_t>& rFlips);

} // namespace ac
