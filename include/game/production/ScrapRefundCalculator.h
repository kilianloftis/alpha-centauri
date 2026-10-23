#pragma once

#include "game/production/ConstructableKind.h"
#include "game/effects/ActiveEffect.h"
#include "game/production/ProductionConfigParser.h"

#include <vector>

namespace ac
{

class LuaRuntime;

// Outcome of pricing a player scrap. bAvailable false means the kind has no default_scrap.
struct ScrapQuote_t
{
    bool bAvailable = false;
    int amount = 0;
    StatId_t refundType = StatId_t::EnergyCredits;
};

// Player-scrap pricing from the kind's default_scrap and any item override, scaled by
// ScrapRefund modifiers and capped by the kind's ceiling. ScrapPayout decides where it lands.
class ScrapRefundCalculator
{
public:
    // rConfig and rLua outlive every base (GameDataContext owns both).
    ScrapRefundCalculator(const ProductionConfig_t& rConfig, LuaRuntime& rLua);
    ~ScrapRefundCalculator() = default;

    // Unavailable without a default_scrap or with a cleared formula. Throws on a negative cost or
    // amount.
    ScrapQuote_t Quote(int mineralCost,
                       ConstructableKind_t kind,
                       const ScrapOverride_t& rOverride,
                       const std::vector<ActiveEffect_t>& rBonusEffects) const;

private:
    const ScrapKindConfig_t* FindKind_(ConstructableKind_t kind) const;
    // The kind's or override's formula at this cost; throws on a negative result.
    int FormulaRefund_(const std::string& rFormula, int mineralCost) const;
    // ScrapRefund modifiers on top of the formula; throws on a negative result.
    static int WithRefundBonus_(int formulaAmount,
                                const std::vector<ActiveEffect_t>& rBonusEffects);

    const ProductionConfig_t* m_pConfig;
    LuaRuntime* m_pLua;
};

} // namespace ac
