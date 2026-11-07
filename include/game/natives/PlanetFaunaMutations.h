#pragma once

#include <random>

namespace ac
{

class GameState;

// Per-turn Planet fauna (SMAC alien_fauna unit half): boats, fungus lurkers, optional swarm.
void RunPlanetFauna(GameState& rGameState, std::mt19937& rRng);

} // namespace ac
