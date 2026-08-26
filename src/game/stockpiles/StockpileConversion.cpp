#include "game/stockpiles/StockpileConversion.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include "game/stockpiles/StockpileConfig.h"

#include <cmath>
#include <ranges>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

// ThisBase modifiers for `statId` drawn from the stockpile's own effects: the
// MineralsConverted ones (scaled by ctx.pStockpile) plus the percentage / multiplier ops that
// scale them. Tile selectors and other amount sources stay out — a selector modifier belongs
// to tile-yield resolution, and ElevationEnergySeed has no meaning without a tile.
auto FilterStockpileYield_(const std::vector<ActiveEffect_t>& rEffects, StatId_t statId,
                           const EffectContext_t& rCtx)
{
    return rEffects | std::views::filter([statId, &rCtx](const ActiveEffect_t& rEffect)
    {
        const StatModifierEffect_t* pStatModifier =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (!pStatModifier || pStatModifier->stat != statId || pStatModifier->selector)
        {
            return false;
        }
        if (pStatModifier->amountSource
            && *pStatModifier->amountSource
                   != StatModifierEffect_t::AmountSource_t::MineralsConverted)
        {
            return false;
        }
        if (!rEffect.config->condition)
        {
            return true;
        }
        return ConditionSatisfied(*rEffect.config, rCtx, rEffect.originBase);
    });
}

int RoundYield_(double total, StockpileRounding_t rounding)
{
    switch (rounding)
    {
    case StockpileRounding_t::Down:
        return static_cast<int>(std::floor(total));
    case StockpileRounding_t::Up:
        return static_cast<int>(std::ceil(total));
    case StockpileRounding_t::Nearest:
        return static_cast<int>(std::lround(total));
    }
    return 0;
}

int StatYield_(const std::vector<ActiveEffect_t>& rEffects, StatId_t stat,
               const EffectContext_t& rCtx, StockpileRounding_t rounding)
{
    const double total =
        ResolveStatModifiers(FilterStockpileYield_(rEffects, stat, rCtx), 0.0, &rCtx).total;
    if (total <= 0.0)
    {
        return 0;
    }
    return RoundYield_(total, rounding);
}

} // namespace

void ApplyStockpileConversionAtBase(BaseManager& rBase, const StockpileConfig_t& rStockpile,
                                    int minerals)
{
    if (minerals <= 0)
    {
        return;
    }

    std::vector<ActiveEffect_t> effects;
    AppendActiveEffects(rStockpile.effects, &rBase, rStockpile.id, effects);

    const StockpileConversionSubject_t stockpile{minerals};
    const EffectContext_t ctx{.pBase = &rBase, .pStockpile = &stockpile};
    for (const StatId_t stat : k_StockpileOutputStats)
    {
        const int yield = StatYield_(effects, stat, ctx, rStockpile.rounding);
        if (yield <= 0)
        {
            continue;
        }
        if (stat == StatId_t::Energy)
        {
            // Not a bank: converted energy takes the same route collected energy does.
            rBase.GetResources().AddAllocatedEnergy(yield);
            continue;
        }
        rBase.GetResources().AddResource(stat, yield);
    }
}

} // namespace ac
