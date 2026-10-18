#include "game/effects/TriggeredEffectRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

bool HoldOrderActive_(const Unit& rUnit)
{
    const std::optional<UnitOrder_t>& rOrder = rUnit.GetOrder();
    return rOrder.has_value() && std::holds_alternative<HoldOrder_t>(*rOrder);
}

const BaseManager* OwnBaseAt_(const GameState& rGameState, const Unit& rUnit)
{
    const BaseManager* pBase =
        rGameState.FindBaseAt(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
    if (!pBase || pBase->GetFaction().GetFactionId() != rUnit.GetFaction().GetFactionId())
    {
        return nullptr;
    }
    return pBase;
}

bool HoldEffectMatches_(const TriggeredEffectConfig_t& rEffect, const Unit& rUnit,
                        const BaseManager& rBase)
{
    if (!rEffect.condition)
    {
        return true;
    }
    EffectContext_t ctx;
    ctx.pUnit = &rUnit;
    ctx.pBase = &rBase;
    ctx.pFaction = &rUnit.GetFaction();
    ctx.targetTile = &rUnit.GetTile();
    return ConditionSatisfied(*rEffect.condition, ctx);
}

void CollectBaseHasBuilding_(const Condition_t& rCondition, std::vector<std::string>& rIds)
{
    std::visit(
        [&](const auto& rAlt)
        {
            using T = std::decay_t<decltype(rAlt)>;
            if constexpr (std::is_same_v<T, BaseHasBuilding_t>)
            {
                rIds.push_back(rAlt.buildingId);
            }
            else if constexpr (std::is_same_v<T, AllOf_t> || std::is_same_v<T, AnyOf_t>)
            {
                for (const Condition_t& rNested : rAlt.conditions)
                {
                    CollectBaseHasBuilding_(rNested, rIds);
                }
            }
            else
            {
                (void)rAlt;
            }
        },
        rCondition.AsVariant());
}

std::string HostNameForEffect_(const TriggeredEffectConfig_t& rEffect, const BaseManager& rBase)
{
    if (rEffect.condition)
    {
        std::vector<std::string> buildingIds;
        CollectBaseHasBuilding_(*rEffect.condition, buildingIds);
        for (const std::string& rId : buildingIds)
        {
            if (const BuildingConfig_t* pBuilding = rBase.GetBuildingManager().FindBuilding(rId))
            {
                return pBuilding->name.empty() ? pBuilding->id : pBuilding->name;
            }
        }
    }
    return rBase.GetName();
}

} // namespace

bool ImprovementHasVisitEffects(const ImprovementConfig_t& rHost)
{
    return !rHost.onVisitEffects.empty();
}

bool TileHasVisitEffects(const Tile& rTile)
{
    return rTile.ForEachOccupant(ImprovementHasVisitEffects);
}

bool UnitHasHoldLink(const Unit& rUnit)
{
    const GameState& rGameState = CurrentGameState();
    if (!HoldOrderActive_(rUnit))
    {
        return false;
    }
    const BaseManager* pBase = OwnBaseAt_(rGameState, rUnit);
    if (!pBase)
    {
        return false;
    }
    for (const TriggeredEffectConfig_t& rEffect : rUnit.GetDesign().CollectOnHoldEffects())
    {
        if (HoldEffectMatches_(rEffect, rUnit, *pBase))
        {
            return true;
        }
    }
    return false;
}

std::string HoldLinkHostName(const Unit& rUnit)
{
    const GameState& rGameState = CurrentGameState();
    if (!HoldOrderActive_(rUnit))
    {
        return {};
    }
    const BaseManager* pBase = OwnBaseAt_(rGameState, rUnit);
    if (!pBase)
    {
        return {};
    }
    for (const TriggeredEffectConfig_t& rEffect : rUnit.GetDesign().CollectOnHoldEffects())
    {
        if (HoldEffectMatches_(rEffect, rUnit, *pBase))
        {
            return HostNameForEffect_(rEffect, *pBase);
        }
    }
    return {};
}

bool UnitCanDetonate(const Unit& rUnit)
{
    return !rUnit.GetDesign().CollectOnDetonateEffects().empty();
}

int PredictModifyPopulationDelta(int size, const ModifyPopulationEffect_t& rEffect)
{
    int delta = 0;
    switch (rEffect.op)
    {
    case ModifierOp_t::Add:
        delta = rEffect.amount;
        break;
    case ModifierOp_t::AddPercent:
        delta = (size * rEffect.amount) / 100;
        break;
    case ModifierOp_t::MultiplyGeometric:
        throw std::runtime_error(
            "PredictModifyPopulationDelta: MultiplyGeometric is not supported");
    case ModifierOp_t::MaxClamp:
    case ModifierOp_t::MinClamp:
        throw std::runtime_error(
            "PredictModifyPopulationDelta: clamp ops are not supported");
    }

    if (delta < 0)
    {
        const int toRemove = -delta;
        const int canRemove = std::max(0, size - rEffect.minSize);
        return -std::min(toRemove, canRemove);
    }
    return delta;
}

int PredictTriggeredPopulationSize(std::span<const TriggeredEffectConfig_t> rEffects, int size)
{
    int current = size;
    for (const TriggeredEffectConfig_t& rEffect : rEffects)
    {
        const ModifyPopulationEffect_t* pModify =
            std::get_if<ModifyPopulationEffect_t>(&rEffect.effect);
        if (!pModify)
        {
            continue;
        }
        current += PredictModifyPopulationDelta(current, *pModify);
    }
    return current;
}

int PredictUnitProductionPopulationSize(const UnitDesign& rDesign, int size)
{
    int current = size;
    for (const UnitComponentConfig_t* pComp : rDesign.GetComponents())
    {
        if (!pComp)
        {
            continue;
        }
        current = PredictTriggeredPopulationSize(pComp->onCompleteEffects, current);
    }
    return current;
}

} // namespace ac
