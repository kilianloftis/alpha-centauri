#pragma once

#include "game/effects/EffectEnums.h"

#include <optional>
#include <string>

namespace ac
{

// Scrap defaults for one IConstructable kind, from production.json
// kinds.<kind>.default_scrap. A kind with no block cannot be scrapped.
struct ScrapKindConfig_t
{
    // Lua expression returning a whole refund amount. Variable `minerals` is the item's
    // listed mineral cost. A non-integer result is a config error (use floor).
    std::string formula;
    // IsScrapRefundStat whitelist: energy_credits, minerals, nutrients, energy, econ, labs,
    // psych.
    StatId_t refundType = StatId_t::EnergyCredits;
    // Cap after formula/override and ScrapRefund bonuses: floor(listed * percent / 100).
    // Required in JSON (no silent 100). 0 pays nothing; values above 100 are legal for mods.
    // Deliberately not overridable: the ceiling is the kind's policy, so an item override
    // cannot mint a refund larger than the kind allows.
    int refundCeilingPercent = 0;
};

// Partial scrap override on a unit component or building. Specified keys replace
// kinds.<kind>.default_scrap; omitted keys keep the kind default. JSON `"formula": null`
// stores an empty string and denies scrap — Quote is then unavailable, same as a kind with
// no default_scrap.
struct ScrapOverride_t
{
    std::optional<std::string> formula;
    std::optional<StatId_t> refundType;
};

inline bool ScrapOverrideDenies(const ScrapOverride_t& rOverride)
{
    return rOverride.formula.has_value() && rOverride.formula->empty();
}

// Layer rFrom over rInto key by key: a key rFrom leaves unset keeps rInto's value. Unit
// designs fold every component's override this way, so the last occupied slot wins per key.
inline void MergeScrapOverride(const ScrapOverride_t& rFrom, ScrapOverride_t& rInto)
{
    if (rFrom.formula)
    {
        rInto.formula = rFrom.formula;
    }
    if (rFrom.refundType)
    {
        rInto.refundType = rFrom.refundType;
    }
}

// Stats a scrap refund can be paid in. ParseStatId accepts every stat; refund_type is this
// subset (treasury, production stockpile, resource banks, or allocated energy).
inline bool IsScrapRefundStat(StatId_t stat)
{
    switch (stat)
    {
        case StatId_t::EnergyCredits:
        case StatId_t::Minerals:
        case StatId_t::Nutrients:
        case StatId_t::Energy:
        case StatId_t::Econ:
        case StatId_t::Labs:
        case StatId_t::Psych:
            return true;
        default:
            return false;
    }
}

// True when the payout has to land in a base. Energy credits go to the faction treasury.
inline bool ScrapRefundNeedsBase(StatId_t stat)
{
    return IsScrapRefundStat(stat) && stat != StatId_t::EnergyCredits;
}

} // namespace ac
