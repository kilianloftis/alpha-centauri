#pragma once

#include "game/map/ElevationChange.h"

#include <vector>

namespace ac
{

class IUnitOrderWorld;
class TileEffectsContext;

// Building id that lets a land-domain base remain when its tile becomes water.
inline constexpr const char* k_PressureDomeBuildingId = "Pressure_Dome";

// Drop improvements whose domain is not the new surface, and raze a base that may not
// occupy water. An omitted domain stays. A base that MayOccupyWater keeps its Base marker.
// pWorld may be null: improvements are still removed, and a base that must be razed throws.
void ReconcileSurfaceFlips(TileEffectsContext& rTileEffects, IUnitOrderWorld* pWorld,
                           const std::vector<SurfaceFlip_t>& rFlips);

} // namespace ac
