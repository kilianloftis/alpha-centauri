#include "game/effects/InteractionResolve.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/Faction.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"

namespace ac
{
namespace
{

bool AxisMatches_(const std::optional<UnitDomain_t>& rOpt, UnitDomain_t value)
{
    return !rOpt.has_value() || *rOpt == value;
}

bool AxisMatches_(const std::optional<InteractionSurface_t>& rOpt, InteractionSurface_t value)
{
    return !rOpt.has_value() || *rOpt == value;
}

bool AxisMatches_(const std::optional<InteractionFooting_t>& rOpt, InteractionFooting_t value)
{
    return !rOpt.has_value() || *rOpt == value;
}

// The row axis is shared by every grid; only the column depends on which grid it is.
bool ColumnMatches_(const InteractionOverrideEffect_t& rOverride,
                    const InteractionQuery_t& rQuery)
{
    switch (rQuery.grid)
    {
    case InteractionGridId_t::Enter:
        return AxisMatches_(rOverride.surface, rQuery.surface);
    case InteractionGridId_t::AttackTile:
        return AxisMatches_(rOverride.footing, rQuery.footing);
    case InteractionGridId_t::AttackUnit:
    case InteractionGridId_t::Zoc:
        return AxisMatches_(rOverride.targetDomain, rQuery.targetDomain);
    }
    return false;
}

bool OverrideMatchesQuery_(const InteractionOverrideEffect_t& rOverride,
                           const InteractionQuery_t& rQuery)
{
    return rOverride.grid == rQuery.grid
        && AxisMatches_(rOverride.actorDomain, rQuery.actorDomain)
        && ColumnMatches_(rOverride, rQuery);
}

std::optional<InteractionCell_t> FirstNonDefaultOverride_(
    const std::vector<ActiveEffect_t>& rEffects, const InteractionQuery_t& rQuery,
    const EffectContext_t& rCtx, InteractionCell_t stock)
{
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        const InteractionOverrideEffect_t* pOverride =
            std::get_if<InteractionOverrideEffect_t>(&rEffect.config->effect);
        if (!pOverride || !OverrideMatchesQuery_(*pOverride, rQuery))
        {
            continue;
        }
        if (!ConditionSatisfied(*rEffect.config, rCtx, rEffect.originBase))
        {
            continue;
        }
        // Non-default only: restating stock is a no-op. For any concrete query exactly one
        // polarity differs from stock, so overlapping matches cannot disagree.
        if (pOverride->cell == stock)
        {
            continue;
        }
        return pOverride->cell;
    }
    return std::nullopt;
}

InteractionCell_t StockCell_(const InteractionGridsConfig_t& rGrids,
                             const InteractionQuery_t& rQuery)
{
    const std::size_t row = UnitDomainIndex(rQuery.actorDomain);
    switch (rQuery.grid)
    {
    case InteractionGridId_t::Enter:
        return rGrids.enter[row][SurfaceIndex(rQuery.surface)];
    case InteractionGridId_t::AttackTile:
        return rGrids.attackTile[row][FootingIndex(rQuery.footing)];
    case InteractionGridId_t::AttackUnit:
        return rGrids.attackUnit[row][UnitDomainIndex(rQuery.targetDomain)];
    case InteractionGridId_t::Zoc:
        return rGrids.zoc[row][UnitDomainIndex(rQuery.targetDomain)];
    }
    return InteractionCell_t::Deny;
}

} // namespace

InteractionGridMask_t InteractionMaskOf(std::span<const EffectConfig_t> effects)
{
    InteractionGridMask_t mask = 0;
    for (const EffectConfig_t& rEffect : effects)
    {
        if (const auto* pOverride = std::get_if<InteractionOverrideEffect_t>(&rEffect.effect))
        {
            mask |= GridBit(pOverride->grid);
        }
    }
    return mask;
}

InteractionGridMask_t InteractionMaskOf(std::span<const ActiveEffect_t> effects)
{
    InteractionGridMask_t mask = 0;
    for (const ActiveEffect_t& rEffect : effects)
    {
        if (const auto* pOverride =
                std::get_if<InteractionOverrideEffect_t>(&rEffect.config->effect))
        {
            mask |= GridBit(pOverride->grid);
        }
    }
    return mask;
}

bool UnitMayOverride(const Unit& rUnit, InteractionGridId_t grid)
{
    const InteractionGridMask_t mask =
        rUnit.GetDesign().GetInteractionMask() | rUnit.GetFaction().GetInteractionMask();
    return MaskCovers(mask, grid);
}

InteractionFooting_t FootingFor(const Unit& rUnit)
{
    if (rUnit.IsEmbarked())
    {
        return InteractionFooting_t::Embarked;
    }
    return rUnit.GetTile().IsWater() ? InteractionFooting_t::Water : InteractionFooting_t::Land;
}

InteractionCell_t ResolveInteractionCell(const InteractionGridsConfig_t& rGrids,
                                         const InteractionQuery_t& rQuery,
                                         const Unit* pActingUnit,
                                         const EffectContext_t& rCtx)
{
    const InteractionCell_t stock = StockCell_(rGrids, rQuery);
    if (pActingUnit && UnitMayOverride(*pActingUnit, rQuery.grid))
    {
        if (auto cell = FirstNonDefaultOverride_(CollectLiveUnitEffects(*pActingUnit).effects,
                                                 rQuery, rCtx, stock))
        {
            return *cell;
        }
    }
    return stock;
}

} // namespace ac
