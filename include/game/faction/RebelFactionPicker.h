#pragma once

#include "game/population/pop-types/PopCompositionConfigParser.h"

#include <random>

namespace ac
{

class BaseManager;
class GameState;

// Weighted pick of another alive faction for a rebelling base, then transfer ownership.
// No-op (with a log) when there are no candidates.
void PickRebelFactionAndTransfer(BaseManager& rBase, GameState& rGameState,
                                 const RebelSelectionConfig_t& rConfig, std::mt19937& rRng);

} // namespace ac
