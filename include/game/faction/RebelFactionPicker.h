#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"

#include <optional>
#include <random>

namespace ac
{

class BaseManager;
class GameState;

// Weighted pick of another alive faction for a rebelling base, then transfer ownership.
// Returns the new owner, or nullopt (with a log) when there were no candidates.
std::optional<FactionId_t> PickRebelFactionAndTransfer(BaseManager& rBase, GameState& rGameState,
                                                       const RebelSelectionConfig_t& rConfig,
                                                       std::mt19937& rRng);

} // namespace ac
