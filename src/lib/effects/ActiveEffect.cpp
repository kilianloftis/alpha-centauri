#include "lib/effects/ActiveEffect.h"

#include "game/IEffectsProvider.h"
#include "game/Faction.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/units/UnitComponentConfig.h"
#include "lib/effects/BonusEffect.h"
#include <algorithm>
#include <cmath>
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
        activeEffect.originBase = (rEffect.scope == EffectScope_t::ThisBase) ? pOriginBase : nullptr;
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
    return LaneFor(rEffect.scope) == EffectLane::TileLocal
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
            continue;
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
            // AllOwnerBases / FactionGlobal grant: non-ThisBase effects apply once globally;
            // ThisBase-scoped sub-effects are cloned once per base.
            const std::pair<const BaseManager*, std::string> globalKey = {nullptr, pGrant->buildingId};
            if (!processedGrantedIds.count(globalKey))
            {
                processedGrantedIds.insert(globalKey);
                AppendActiveEffectsIf_(pGranted->effects, nullptr, sourceId,
                                       [](const EffectConfig_t& rEffect)
                                       { return rEffect.scope != EffectScope_t::ThisBase; },
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
                                           [](const EffectConfig_t& rEffect)
                                           { return rEffect.scope == EffectScope_t::ThisBase; },
                                           effects);
                }
            }
        }
    }

    return effects;
}

double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp>>& contributions)
{
    double addTotal = base;
    double arithmeticFactor = 1.0;
    double geometricFactor = 1.0;
    for (const auto& [amount, op] : contributions)
    {
        switch (op)
        {
            case ModifierOp::Add:               addTotal += amount; break;
            case ModifierOp::AddPercent:        arithmeticFactor += amount / 100.0; break;
            case ModifierOp::MultiplyGeometric: geometricFactor *= amount; break;
        }
    }
    return addTotal * arithmeticFactor * geometricFactor;
}

FactionEffects_t CollectActiveEffects(const IEffectsProvider& rProvider)
{
    return rProvider.GetActiveEffects();
}

StatBreakdown_t ResolveStatModifiers(const std::vector<ActiveEffect_t>& matching, double baseValue)
{
    StatBreakdown_t breakdown;
    breakdown.total = 0.0;

    for (const ActiveEffect_t& active : matching)
    {
        if (!active.config)
        {
            continue;
        }

        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&active.config->effect);
        if (!pStatModifier)
        {
            continue;
        }

        StatBreakdown_t::Contribution contribution;
        contribution.sourceId = active.sourceId;
        contribution.amount = pStatModifier->amount;
        contribution.op = pStatModifier->op;
        breakdown.contributions.push_back(contribution);
    }

    std::sort(breakdown.contributions.begin(), breakdown.contributions.end(),
              [](const StatBreakdown_t::Contribution& a, const StatBreakdown_t::Contribution& b)
              {
                  return a.sourceId < b.sourceId;
              });

    std::vector<std::pair<double, ModifierOp>> stack;
    stack.reserve(breakdown.contributions.size());
    for (const StatBreakdown_t::Contribution& c : breakdown.contributions)
    {
        stack.emplace_back(c.amount, c.op);
    }
    breakdown.total = ApplyModifierStack(baseValue, stack);
    return breakdown;
}

bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx)
{
    if (!config.condition)
    {
        return true;
    }
    const Condition_t& condition = *config.condition;
    switch (condition.kind)
    {
        case ConditionKind::TargetTileHas:
            return ctx.targetTile != nullptr && ctx.targetTile->HasFeature(condition.value);
    }
    return false;
}

std::vector<ActiveEffect_t> FilterByStatId(const std::vector<ActiveEffect_t>& effects, StatId statId)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& effect : effects)
    {
        if (!effect.config)
        {
            continue;
        }
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        // Conditional effects are excluded from context-free resolution — they only apply
        // through FilterByStatIdInContext with a context that satisfies the condition.
        if (pStatModifier && pStatModifier->stat == statId && !effect.config->condition)
        {
            matching.push_back(effect);
        }
    }
    return matching;
}

std::vector<ActiveEffect_t> FilterByStatIdInContext(const std::vector<ActiveEffect_t>& effects,
                                                    StatId statId, const EffectContext_t& ctx)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& effect : effects)
    {
        if (!effect.config)
        {
            continue;
        }
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        if (pStatModifier && pStatModifier->stat == statId && ConditionSatisfied(*effect.config, ctx))
        {
            matching.push_back(effect);
        }
    }
    return matching;
}

std::vector<ActiveEffect_t> FilterFlatByStatId(const BaseEffects_t& rBaseEffects, StatId statId)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& effect : rBaseEffects.effects)
    {
        if (!effect.config)
        {
            continue;
        }
        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        if (pStatModifier && pStatModifier->stat == statId && !pStatModifier->selector && !effect.config->condition)
        {
            matching.push_back(effect);
        }
    }
    return matching;
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
            case EffectLane::Base:
                if (effect.originBase == &rBase)
                {
                    matching.effects.push_back(effect);
                }
                break;
            case EffectLane::FactionWide:
                matching.effects.push_back(effect);
                break;
            case EffectLane::FactionUnits:
            case EffectLane::UnitLocal:
            case EffectLane::PopLocal:
            case EffectLane::TileLocal:
                // Resolved by their own unit/pop/tile; never apply to base-level calculations.
                break;
        }
    }
    return matching;
}

std::vector<ActiveEffect_t> FilterByScope(const std::vector<ActiveEffect_t>& effects, EffectScope_t scope)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& effect : effects)
    {
        if (effect.config && effect.config->scope == scope)
        {
            matching.push_back(effect);
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
        const std::vector<ActiveEffect_t> flatEffects =
            FilterByScope(CollectPopEffects(rPop.GetConfig()), EffectScope_t::ThisBase);

        for (const ActiveEffect_t& effect : flatEffects)
        {
            ActiveEffect_t active = effect;
            active.originBase = &rOriginBase;
            result.push_back(active);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile, const ImprovementRegistry& rImprovements)
{
    std::vector<ActiveEffect_t> result;

    // Terrain features (rockiness/moisture/river/fungus) are resolved by string id against
    // the registry; improvements are already held as config pointers, so iterate them directly.
    // Own-tile collection is the distance-0 case of the shared tile-reach filter.
    for (const std::string& featureId : rTile.GetTerrainFeatureIds())
    {
        if (const ImprovementConfig_t* pFeature = rImprovements.Find(featureId))
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

} // namespace ac
