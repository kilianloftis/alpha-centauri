#pragma once

#include "game/diplomacy/DiplomacyLedger.h"
#include "game/base/BaseTypes.h"

#include <unordered_map>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;

// One trading partner that contributes commerce to a specific base this turn.
struct CommercePartnerLine_t
{
    const Faction* pPartner = nullptr;
    DiplomaticStatus_t status = DiplomaticStatus_t::Neutral;
    int ourEnergy = 0;
    int theirEnergy = 0;
};

// One planet-wide pass that ranks each faction's bases once, so prefer it to a loop over
// ComputeCommerceForBase. Bases with no eligible pair are absent.
std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> ComputeCommerceLines(
    const Faction& rOwner);

// Missing keys mean 0.
std::unordered_map<BaseId_t, int> ComputeCommerceForFaction(const Faction& rOwner);

// Empty for a base that is momentarily ownerless (mid CreateBaseFromSnapshot): that is no
// commerce, not an error. Per-base callers go through CommerceCache, which memoizes the pass.
std::vector<CommercePartnerLine_t> ComputeCommerceForBase(const BaseManager& rBase);

} // namespace ac
