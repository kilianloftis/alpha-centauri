#pragma once

#include "game/faction/base/BaseTypes.h"

#include <unordered_map>

namespace ac
{

class Faction;
class GameState;

// Pure commerce income math: pairs Friendship/Pact bases by pre-commerce energy and returns
// per-base commerce energy for the owning faction. Does not mutate ResourceManager or treasury.
// TODO: zero commerce when sanctions are in effect against either faction.
class CommerceCalculator
{
public:
    CommerceCalculator() = default;

    // Per-base commerce energy for rOwner this turn (missing keys mean 0).
    std::unordered_map<BaseId_t, int> ComputeForFaction(const Faction& rOwner,
                                                        const GameState& rGameState) const;
};

} // namespace ac
