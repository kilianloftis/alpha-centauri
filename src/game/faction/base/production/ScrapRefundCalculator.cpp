#include "game/faction/base/production/ScrapRefundCalculator.h"
#include "lib/LuaRuntime.h"

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

ScrapQuote_t ScrapRefundCalculator::Quote(int mineralCost, ConstructableKind_t kind) const
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

    const std::unordered_map<std::string, int> vars = {{"minerals", mineralCost}};
    const int amount = m_pLua->EvalInt(pKind->formula, vars);
    if (amount < 0)
    {
        throw std::runtime_error("Scrap formula '" + pKind->formula + "' produced "
                                 + std::to_string(amount) + "; refund must not be negative");
    }

    ScrapQuote_t quote;
    quote.bAvailable = true;
    quote.amount = amount;
    quote.refundType = pKind->refundType;
    return quote;
}

const ScrapKindConfig_t* ScrapRefundCalculator::FindKind_(ConstructableKind_t kind) const
{
    const auto it = m_pConfig->kinds.find(kind);
    if (it == m_pConfig->kinds.end() || !it->second.scrap)
    {
        return nullptr;
    }
    return &*it->second.scrap;
}

} // namespace ac
