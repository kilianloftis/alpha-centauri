#include "game/production/ScrapRefundCalculator.h"
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
    if (ScrapOverrideDenies(rOverride))
    {
        return {};
    }

    const std::string& rFormula = rOverride.formula ? *rOverride.formula : pKind->formula;
    const int formulaAmount = FormulaRefund_(rFormula, mineralCost);
    const int bonusAmount = WithRefundBonus_(formulaAmount, rBonusEffects);
    const int ceiling = mineralCost * pKind->refundCeilingPercent / 100;
    const int amount = std::min(bonusAmount, ceiling);

    ScrapQuote_t quote;
    quote.bAvailable = true;
    quote.amount = amount;
    quote.refundType = rOverride.refundType ? *rOverride.refundType : pKind->refundType;
    return quote;
}

int ScrapRefundCalculator::FormulaRefund_(const std::string& rFormula, int mineralCost) const
{
    const std::unordered_map<std::string, double> vars = {
        {"minerals", static_cast<double>(mineralCost)}};
    const int amount = m_pLua->EvalInt(rFormula, vars);
    if (amount < 0)
    {
        throw std::runtime_error("Scrap formula '" + rFormula + "' produced "
                                 + std::to_string(amount) + "; refund must not be negative");
    }
    return amount;
}

int ScrapRefundCalculator::WithRefundBonus_(int formulaAmount,
                                            const std::vector<ActiveEffect_t>& rBonusEffects)
{
    const double seed = static_cast<double>(formulaAmount);
    const double bonusTotal =
        ResolveStatModifiers(FilterByStatId(rBonusEffects, StatId_t::ScrapRefund), seed, nullptr)
            .total;
    const int amount = FinalizeResolvedStat(bonusTotal);
    if (amount < 0)
    {
        throw std::runtime_error("ScrapRefund modifiers produced " + std::to_string(amount)
                                 + "; refund must not be negative");
    }
    return amount;
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
