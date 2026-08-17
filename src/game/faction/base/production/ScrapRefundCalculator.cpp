#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "game/effects/EffectEnums.h"
#include "lib/LuaRuntime.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ac
{

ScrapRefundCalculator::ScrapRefundCalculator(const ProductionConfig_t& rConfig, LuaRuntime& rLua)
    : m_pConfig(&rConfig)
    , m_pLua(&rLua)
{
}

ScrapQuote_t ScrapRefundCalculator::Quote(int mineralCost,
                                          ConstructableKind_t kind,
                                          const ScrapOverride_t& rOverride,
                                          const std::vector<ActiveEffect_t>& rBonusEffects) const
{
    if (mineralCost < 0)
    {
        throw std::invalid_argument("ScrapRefundCalculator::Quote: mineralCost "
                                    + std::to_string(mineralCost) + " must not be negative");
    }
    const ScrapKindConfig_t* pKind = FindKind_(kind);
    if (!pKind)
    {
        return {};
    }

    const std::string& rFormula = rOverride.formula ? *rOverride.formula : pKind->formula;
    const std::unordered_map<std::string, int> vars = {{"minerals", mineralCost}};
    const int formulaAmount = m_pLua->EvalInt(rFormula, vars);
    if (formulaAmount < 0)
    {
        throw std::runtime_error("Scrap formula '" + rFormula + "' produced "
                                 + std::to_string(formulaAmount)
                                 + "; refund must not be negative");
    }

    const int bonusAmount = FinalizeResolvedStat(
        ResolveStatModifiers(FilterByStatId(rBonusEffects, StatId_t::ScrapRefund),
                             static_cast<double>(formulaAmount))
            .total);
    if (bonusAmount < 0)
    {
        throw std::runtime_error("ScrapRefund modifiers produced "
                                 + std::to_string(bonusAmount) + "; refund must not be negative");
    }

    const int ceiling = mineralCost * pKind->refundCeilingPercent / 100;
    const int amount = std::min(bonusAmount, ceiling);

    ScrapQuote_t quote;
    quote.bAvailable = true;
    quote.amount = amount;
    quote.refundType = rOverride.refundType ? *rOverride.refundType : pKind->refundType;
    return quote;
}

const ScrapKindConfig_t* ScrapRefundCalculator::FindKind_(ConstructableKind_t kind) const
{
    const auto it = m_pConfig->kinds.find(kind);
    if (it == m_pConfig->kinds.end() || !it->second.defaultScrap)
    {
        return nullptr;
    }
    return &*it->second.defaultScrap;
}

} // namespace ac
