#include "game/effects/ActiveEffect.h"

#include "game/IEffectsProvider.h"
#include "game/Faction.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/effects/BonusEffect.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <set>
#include <string_view>

namespace ac
{

namespace
{

// The one config->ActiveEffect_t loop. Every public collect/append helper funnels through
// here so the Instantaneous exclusion (those fire once via DispatchInstantaneousEffects)
// and the ThisBase origin tagging can never be forgotten by an individual collector.
template <typename IncludePred>
void AppendActiveEffectsIf_(const std::vector<EffectConfig_t>& rEffects,
                            const BaseManager* pOriginBase,
                            const std::string& sourceId,
                            IncludePred include,
                            std::vector<ActiveEffect_t>& rOut)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        if (rEffect.persistence == EffectPersistence_t::Instantaneous)
            continue;
        if (!include(rEffect))
            continue;
        ActiveEffect_t activeEffect;
        activeEffect.config = &rEffect;
        activeEffect.sourceId = sourceId;
        // ThisBase / ProducedAtThisBase need origin for routing. FactionUnits keep origin for
        // per-base conditions (e.g. OriginBaseIsTargetBase) — not as a membership filter.
        const EffectLane_t lane = LaneFor(rEffect.scope);
        const bool bTagOrigin = lane == EffectLane_t::Base
            || lane == EffectLane_t::ProducedAtBase
            || rEffect.scope == EffectScope_t::FactionUnits;
        activeEffect.originBase = bTagOrigin ? pOriginBase : nullptr;
        rOut.push_back(activeEffect);
    }
}

} // namespace

void AppendActiveEffects(const std::vector<EffectConfig_t>& rEffects,
                         const BaseManager* pOriginBase,
                         const std::string& sourceId,
                         std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, pOriginBase, sourceId,
                           [](const EffectConfig_t&) { return true; }, rOut);
}

void AppendFactionLaneEffects(const std::vector<EffectConfig_t>& rEffects,
                              const std::string& sourceId,
                              std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, nullptr, sourceId,
                           [](const EffectConfig_t& rEffect) { return IsFactionLane(rEffect.scope); },
                           rOut);
}

bool TileEffectReaches(const EffectConfig_t& rEffect, int distance)
{
    return LaneFor(rEffect.scope) == EffectLane_t::TileLocal
        && rEffect.persistence != EffectPersistence_t::Instantaneous
        && rEffect.radius >= distance;
}

void AppendTileEffects(const std::vector<EffectConfig_t>& rEffects,
                       const std::string& sourceId,
                       int distance,
                       std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, nullptr, sourceId,
                           [distance](const EffectConfig_t& rEffect)
                           { return TileEffectReaches(rEffect, distance); },
                           rOut);
}

namespace
{

// True if buildingId appears as a segment of a chained sourceId ("a -> b -> c").
// Used to break grant cycles: a grant whose target is already an ancestor in its own
// chain (the constructed root or an earlier grant) is already contributing its effects,
// so expanding it again would duplicate them.
bool GrantChainContains_(const std::string& rSourceChain, const std::string& rBuildingId)
{
    const std::string_view chain(rSourceChain);
    const std::string_view separator(" -> ");
    size_t pos = 0;
    while (true)
    {
        const size_t next = chain.find(separator, pos);
        const std::string_view segment = chain.substr(pos, (next == std::string_view::npos ? chain.size() : next) - pos);
        if (segment == rBuildingId)
        {
            return true;
        }
        if (next == std::string_view::npos)
        {
            return false;
        }
        pos = next + separator.size();
    }
}

} // namespace

std::vector<ActiveEffect_t> ExpandGrantBuildingEffects(
    std::vector<ActiveEffect_t> effects,
    const BuildingRegistry& rRegistry,
    const std::vector<const BaseManager*>& rBases)
{
    // Key: (originBase*, grantedBuildingId). Pointer identity is intentional — two
    // different bases granting the same building must each expand independently.
    std::set<std::pair<const BaseManager*, std::string>> processedGrantedIds;

    for (size_t i = 0; i < effects.size(); ++i)
    {
        const EffectConfig_t* pEffect = effects[i].config;
        if (!pEffect)
        {
            continue;
        }

        const GrantBuildingEffect_t* pGrant = std::get_if<GrantBuildingEffect_t>(&pEffect->effect);
        if (!pGrant)
        {
            continue;
        }

        const BuildingConfig_t* pGranted = rRegistry.Find(pGrant->buildingId);
        if (!pGranted)
        {
            throw std::runtime_error("Unknown granted building id '" + pGrant->buildingId + "'");
        }

        if (GrantChainContains_(effects[i].sourceId, pGrant->buildingId))
        {
            continue;
        }

        const std::string sourceId = effects[i].sourceId + " -> " + pGranted->id;

        if (effects[i].originBase != nullptr)
        {
            // ThisBase-scoped grant: expand effects once, attributed to the originating base.
            const std::pair<const BaseManager*, std::string> key = {effects[i].originBase, pGrant->buildingId};
            if (!processedGrantedIds.count(key))
            {
                processedGrantedIds.insert(key);
                AppendActiveEffects(pGranted->effects, effects[i].originBase, sourceId, effects);
            }
        }
        else
        {
            // AllOwnerBases / FactionGlobal grant: non-per-base effects apply once globally;
            // ThisBase / ProducedAtThisBase sub-effects are cloned once per base.
            const auto isPerBaseScope = [](EffectScope_t scope)
            {
                return scope == EffectScope_t::ThisBase
                    || scope == EffectScope_t::ProducedAtThisBase;
            };
            const std::pair<const BaseManager*, std::string> globalKey = {nullptr, pGrant->buildingId};
            if (!processedGrantedIds.count(globalKey))
            {
                processedGrantedIds.insert(globalKey);
                AppendActiveEffectsIf_(pGranted->effects, nullptr, sourceId,
                                       [&](const EffectConfig_t& rEffect)
                                       { return !isPerBaseScope(rEffect.scope); },
                                       effects);
            }

            for (const BaseManager* pBase : rBases)
            {
                if (!pBase) continue;
                const std::pair<const BaseManager*, std::string> key = {pBase, pGrant->buildingId};
                if (!processedGrantedIds.count(key))
                {
                    processedGrantedIds.insert(key);
                    AppendActiveEffectsIf_(pGranted->effects, pBase, sourceId,
                                           [&](const EffectConfig_t& rEffect)
                                           { return isPerBaseScope(rEffect.scope); },
                                           effects);
                }
            }
        }
    }

    return effects;
}

double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp_t>>& contributions)
{
    double addTotal = base;
    double arithmeticFactor = 1.0;
    double geometricFactor = 1.0;
    for (const auto& [amount, op] : contributions)
    {
        switch (op)
        {
            case ModifierOp_t::Add:               addTotal += amount; break;
            case ModifierOp_t::AddPercent:        arithmeticFactor += amount / 100.0; break;
            case ModifierOp_t::MultiplyGeometric: geometricFactor *= amount; break;
        }
    }
    return addTotal * arithmeticFactor * geometricFactor;
}

const FactionEffects_t& CollectActiveEffects(const IEffectsProvider& rProvider)
{
    return rProvider.GetActiveEffects();
}

namespace
{

bool ConditionBodySatisfied_(const Condition_t& condition, const EffectContext_t& ctx,
                             const BaseManager* pOriginBase)
{
    switch (condition.kind)
    {
        case ConditionKind_t::TargetTileHas:
            return ctx.targetTile != nullptr && ctx.targetTile->HasFeature(condition.value);
        case ConditionKind_t::IsDefending:
            return ctx.combatRole == CombatRole_t::Defender;
        case ConditionKind_t::OriginBaseIsTargetBase:
            return pOriginBase != nullptr && ctx.targetTile != nullptr
                && pOriginBase->GetX() == ctx.targetTile->GetX()
                && pOriginBase->GetY() == ctx.targetTile->GetY();
        case ConditionKind_t::AllOf:
            for (const std::string& rFeatureId : condition.values)
            {
                if (!ctx.targetTile || !ctx.targetTile->HasFeature(rFeatureId))
                {
                    return false;
                }
            }
            for (const Condition_t& rNested : condition.conditions)
            {
                if (!ConditionBodySatisfied_(rNested, ctx, pOriginBase))
                {
                    return false;
                }
            }
            return !condition.values.empty() || !condition.conditions.empty();
    }
    return false;
}

} // namespace

bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx,
                        const BaseManager* pOriginBase)
{
    if (!config.condition)
    {
        return true;
    }
    return ConditionBodySatisfied_(*config.condition, ctx, pOriginBase);
}

bool UnitFilterSatisfied(const EffectConfig_t& config, const Unit& rUnit)
{
    if (!config.unitFilter)
    {
        return true;
    }
    const UnitFilter_t& filter = *config.unitFilter;
    switch (filter.kind)
    {
        case UnitFilterKind_t::Domain:
            return filter.domain.has_value() && rUnit.GetDomain() == *filter.domain;
        case UnitFilterKind_t::HasComponent:
            return filter.component.has_value()
                && rUnit.GetDesign().HasComponent(*filter.component);
        // Design-only: avoid CollectLiveUnitEffects recursion (HasFlag is evaluated while
        // building that list). Native / probe filters key off chassis/special components.
        case UnitFilterKind_t::HasFlag:
            return filter.flag.has_value() && ResolveFlag(rUnit.GetDesign(), *filter.flag);
    }
    return false;
}

BaseEffects_t FilterForBase(const FactionEffects_t& rFactionEffects, const BaseManager& rBase)
{
    BaseEffects_t matching;
    for (const ActiveEffect_t& effect : rFactionEffects.effects)
    {
        if (!effect.config)
        {
            continue;
        }

        switch (LaneFor(effect.config->scope))
        {
            case EffectLane_t::Base:
                if (effect.originBase == &rBase)
                {
                    matching.effects.push_back(effect);
                }
                break;
            case EffectLane_t::FactionWide:
                matching.effects.push_back(effect);
                break;
            case EffectLane_t::FactionUnits:
            case EffectLane_t::ProducedAtBase:
            case EffectLane_t::UnitLocal:
            case EffectLane_t::PopLocal:
            case EffectLane_t::TileLocal:
                // Resolved by their own unit/pop/tile; never apply to base-level calculations.
                break;
        }
    }
    return matching;
}

std::vector<ActiveEffect_t> CollectUnitEffects(const std::vector<const UnitComponentConfig_t*>& components)
{
    std::vector<ActiveEffect_t> result;
    for (const UnitComponentConfig_t* pComp : components)
    {
        if (pComp)
        {
            AppendActiveEffects(pComp->effects, nullptr, pComp->id, result);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> CollectPopEffects(const PopTypeConfig_t& rConfig)
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(rConfig.effects, nullptr, rConfig.id, result);
    return result;
}

std::vector<ActiveEffect_t> CollectFromPops(const PopulationManager& rPops, const BaseManager& rOriginBase)
{
    std::vector<ActiveEffect_t> result;
    for (const Pop& rPop : rPops.Pops())
    {
        // Bind the source to a named local before filtering it: CollectPopEffects(...) is
        // otherwise a temporary destroyed at the end of its statement, and the range-for
        // below needs flatEffects to survive past that statement.
        const std::vector<ActiveEffect_t> popEffects = CollectPopEffects(rPop.GetConfig());
        auto flatEffectsView = FilterByScope(popEffects, EffectScope_t::ThisBase);
        const std::vector<ActiveEffect_t> flatEffects(flatEffectsView.begin(), flatEffectsView.end());

        for (const ActiveEffect_t& effect : flatEffects)
        {
            ActiveEffect_t active = effect;
            active.originBase = &rOriginBase;
            result.push_back(active);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile)
{
    std::vector<ActiveEffect_t> result;

    // Terrain features and improvements are both held as config pointers on the tile.
    // Own-tile collection is the distance-0 case of the shared tile-reach filter.
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature)
        {
            AppendTileEffects(pFeature->effects, pFeature->id, 0, result);
        }
    }

    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        AppendTileEffects(pImprovement->effects, pImprovement->id, 0, result);
    }

    return result;
}

void DispatchInstantaneousEffects(const BuildingConfig_t& rBuilding, BaseManager& rBase)
{
    for (const EffectConfig_t& effect : rBuilding.effects)
    {
        if (effect.persistence != EffectPersistence_t::Instantaneous)
            continue;

        if (const GrantBuildingEffect_t* pGrant = std::get_if<GrantBuildingEffect_t>(&effect.effect))
        {
            rBase.GetBuildingManager().AddBuilding(pGrant->buildingId);
        }
        else if (std::get_if<GrantTechEffect_t>(&effect.effect))
        {
            std::cerr << "[TODO] Instantaneous GrantTech from '" << rBuilding.id << "' not yet implemented\n";
        }
        else if (std::get_if<GrantUnitEffect_t>(&effect.effect))
        {
            std::cerr << "[TODO] Instantaneous GrantUnit from '" << rBuilding.id << "' not yet implemented\n";
        }
    }
}

// A live unit's full effect list: design components, all FactionUnits (unitFilter only),
// and ProducedAtThisBase effects whose origin matches the unit's production base.
std::vector<ActiveEffect_t> CollectLiveUnitEffects(const Unit& rUnit)
{
    std::vector<ActiveEffect_t> effects = rUnit.GetDesign().CollectEffects();
    const auto& rPool = rUnit.GetFaction().GetActiveEffects().effects;
    auto factionEffects = FilterByScope(rPool, EffectScope_t::FactionUnits);
    effects.insert(effects.end(), factionEffects.begin(), factionEffects.end());
    auto producedEffects = FilterByScope(rPool, EffectScope_t::ProducedAtThisBase);
    effects.insert(effects.end(), producedEffects.begin(), producedEffects.end());

    const BaseManager* pProducedAt = rUnit.GetProducedAtBase();
    std::erase_if(effects, [&](const ActiveEffect_t& rEffect)
    {
        if (!rEffect.config)
        {
            return false;
        }
        if (!UnitFilterSatisfied(*rEffect.config, rUnit))
        {
            return true;
        }
        if (rEffect.config->scope == EffectScope_t::ProducedAtThisBase
            && rEffect.originBase != pProducedAt)
        {
            return true;
        }
        return false;
    });
    return effects;
}

int ResolveStat(const UnitDesign& rDesign, StatId_t statId)
{
    return static_cast<int>(
        ResolveStatModifiers(FilterByStatId(rDesign.CollectEffects(), statId), SeedFor(statId)).total);
}

int ResolveStat(const UnitDesign& rDesign, StatId_t statId, const EffectContext_t& rCtx)
{
    return static_cast<int>(
        ResolveStatModifiers(FilterByStatIdInContext(rDesign.CollectEffects(), statId, rCtx),
                             SeedFor(statId), &rCtx).total);
}

int ResolveAdditiveStat(const UnitDesign& rDesign, StatId_t statId)
{
    // Materialize first: FilterByStatId returns a borrowing view (see its contract).
    const std::vector<ActiveEffect_t> effects = rDesign.CollectEffects();
    double addTotal = SeedFor(statId);
    for (const ActiveEffect_t& rEffect : FilterByStatId(effects, statId))
    {
        const StatModifierEffect_t* pStatModifier =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (pStatModifier && pStatModifier->op == ModifierOp_t::Add)
        {
            addTotal += pStatModifier->amount;
        }
    }
    return static_cast<int>(addTotal);
}

bool ResolveFlag(const UnitDesign& rDesign, RuleFlagId_t flagId)
{
    for (const ActiveEffect_t& rEffect : rDesign.CollectEffects())
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == flagId)
        {
            return true;
        }
    }
    return false;
}

int ResolveStat(const Unit& rUnit, StatId_t statId)
{
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects(rUnit);
    return static_cast<int>(ResolveStatModifiers(FilterByStatId(effects, statId), SeedFor(statId)).total);
}

int ResolveStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx)
{
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects(rUnit);
    return static_cast<int>(
        ResolveStatModifiers(FilterByStatIdInContext(effects, statId, rCtx), SeedFor(statId),
                             &rCtx).total);
}

double ResolveMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                 const EffectContext_t& rCtx)
{
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects(rUnit);
    std::vector<std::pair<double, ModifierOp_t>> contributions;
    for (const ActiveEffect_t& rEffect : FilterByStatIdInContext(effects, statId, rCtx))
    {
        const StatModifierEffect_t* pModifier =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (pModifier && pModifier->op != ModifierOp_t::Add)
        {
            contributions.emplace_back(EffectiveStatModifierAmount(*pModifier, &rCtx), pModifier->op);
        }
    }
    return ApplyModifierStack(baseValue, contributions);
}

bool ResolveFlag(const Unit& rUnit, RuleFlagId_t flagId)
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit))
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == flagId)
        {
            return true;
        }
    }
    return false;
}

bool ResolveFlag(const Faction& rFaction, RuleFlagId_t flagId)
{
    for (const ActiveEffect_t& rEffect : CollectActiveEffects(rFaction).effects)
    {
        if (!rEffect.config)
        {
            continue;
        }
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == flagId)
        {
            return true;
        }
    }
    return false;
}

} // namespace ac
