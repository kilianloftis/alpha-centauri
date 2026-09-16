#pragma once

#include "game/faction/DiplomacyLedger.h"
#include "game/faction/base/BaseTypes.h"

#include <unordered_map>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class GameState;

// One Friendship/Pact partner that contributes commerce to a specific base this turn.
struct CommercePartnerLine_t
{
    const Faction* pPartner = nullptr;
    DiplomaticStatus_t status = DiplomaticStatus_t::None;
    int ourEnergy = 0;
    int theirEnergy = 0;
};

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

    // Partner breakdown for one base (UI). Empty when unbound, unpaired, or no eligible treaties.
    std::vector<CommercePartnerLine_t> ComputeForBase(const BaseManager& rBase,
                                                      const GameState& rGameState) const;
};

} // namespace ac
