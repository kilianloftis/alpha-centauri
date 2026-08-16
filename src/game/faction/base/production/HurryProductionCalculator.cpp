#include "game/faction/base/production/HurryProductionCalculator.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "lib/LuaRuntime.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

HurryProductionCalculator::HurryProductionCalculator(const ProductionConfig_t& rConfig,
                                                     LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

int HurryProductionCalculator::BilledMinerals(int remaining, int stockpile, int mineralThreshold,
                                              int multiplier)
{
    if (remaining <= 0)
    {
        return 0;
    }
    const int below = std::max(0, mineralThreshold - stockpile);
    const int expensive = std::min(remaining, below);
    return remaining + expensive * (multiplier - 1);
}

HurryQuote_t HurryProductionCalculator::Quote(const HurryInputs_t& rInputs) const
{
    HurryQuote_t quote;
    const HurryKindConfig_t* pKind = FindKind_(rInputs.kind);
    if (!pKind)
    {
        return quote;
    }

    quote.bAvailable = true;
    quote.remainingMinerals = std::max(0, rInputs.remainingMinerals);
    quote.billedMinerals = BilledMinerals(quote.remainingMinerals, rInputs.mineralStockpile,
                                          pKind->mineralThreshold, pKind->belowThresholdMultiplier);
    quote.creditCost = FinishCost_(*pKind, quote.remainingMinerals, rInputs.mineralStockpile);
    return quote;
}

HurrySpend_t HurryProductionCalculator::ApplyCredits(const HurryInputs_t& rInputs,
                                                     int requestedCredits) const
{
    if (requestedCredits <= 0)
    {
        throw std::invalid_argument("HurryProductionCalculator::ApplyCredits: requested credits "
                                    + std::to_string(requestedCredits) + " must be positive");
    }

    const HurryKindConfig_t& rKind = RequireKind_(rInputs.kind);
    const int remaining = std::max(0, rInputs.remainingMinerals);
    const int stockpile = rInputs.mineralStockpile;
    const int finish = FinishCost_(rKind, remaining, stockpile);
    if (finish <= 0)
    {
        return {};
    }
    if (requestedCredits >= finish)
    {
        return HurrySpend_t{finish, remaining};
    }

    // lo is always affordable (0 minerals cost nothing), hi never is (the branch above ruled
    // out the full buy), so this closes on the largest affordable count.
    int lo = 0;
    int hi = remaining;
    while (hi - lo > 1)
    {
        const int mid = lo + (hi - lo) / 2;
        if (finish - FinishCost_(rKind, remaining - mid, stockpile + mid) <= requestedCredits)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    if (lo == 0)
    {
        return {};
    }
    return HurrySpend_t{finish - FinishCost_(rKind, remaining - lo, stockpile + lo), lo};
}

const HurryKindConfig_t* HurryProductionCalculator::FindKind_(ConstructableKind_t kind) const
{
    const auto it = m_pConfig->kinds.find(kind);
    if (it == m_pConfig->kinds.end() || !it->second.hurry)
    {
        return nullptr;
    }
    return &*it->second.hurry;
}

const HurryKindConfig_t& HurryProductionCalculator::RequireKind_(ConstructableKind_t kind) const
{
    const HurryKindConfig_t* pKind = FindKind_(kind);
    if (!pKind)
    {
        throw std::runtime_error("Hurry production: kind '"
                                 + std::string(ConstructableKindToString(kind))
                                 + "' has no entry in production.json; it cannot be hurried");
    }
    return *pKind;
}

int HurryProductionCalculator::FinishCost_(const HurryKindConfig_t& rKind, int remaining,
                                           int stockpile) const
{
    if (remaining <= 0)
    {
        return 0;
    }

    const int billed = BilledMinerals(remaining, stockpile, rKind.mineralThreshold,
                                      rKind.belowThresholdMultiplier);
    const std::unordered_map<std::string, int> vars = {
        {"minerals", billed},
        {"remaining", remaining},
        {"stockpile", stockpile},
        {"mineral_threshold", rKind.mineralThreshold},
        {"below_threshold_multiplier", rKind.belowThresholdMultiplier},
    };
    const int cost = m_pLua->EvalInt(rKind.formula, vars);
    if (cost <= 0)
    {
        throw std::runtime_error("Hurry production formula '" + rKind.formula + "' produced "
                                 + std::to_string(cost) + "; credit cost must be positive");
    }
    return cost;
}

} // namespace ac
