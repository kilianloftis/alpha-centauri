#pragma once

#include "game/base/BaseTypes.h"

#include <optional>
#include <random>

namespace ac
{

class BaseManager;
class GameState;

// Rolls a faction within RebelCandidateWeights and transfers the base to it. Returns the new
// owner, or nullopt (with a log) when there were no candidates.
std::optional<FactionId_t> PickRebelFactionAndTransfer(BaseManager& rBase, GameState& rGameState,
                                                       std::mt19937& rRng);

} // namespace ac
