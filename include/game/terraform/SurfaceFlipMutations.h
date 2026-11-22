#pragma once

#include "game/terraform/ElevationMutations.h"

#include <vector>

namespace ac
{

class GameState;

// Drops improvements whose domain no longer matches, razes a base that may not occupy water,
// and destroys units stranded on the wrong surface that no standing base shelters.
void ReconcileSurfaceFlips(GameState& rGameState, const std::vector<SurfaceFlip_t>& rFlips);

} // namespace ac
