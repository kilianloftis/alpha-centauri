#include "lib/effects/ActiveEffect.h"

#include "game/Faction.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopContainer.h"
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

namespace ac
{

namespace
{

void AppendEffects(const std::vector<EffectConfig_t>& rEffects,
                   const BaseManager* pOriginBase,
                   const std::string& sourceId,
                   std::vector<ActiveEffect_t>& rOut)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        // Instantaneous effects fire at construction time via DispatchInstantaneousEffects;
        // they must not enter the continuous active-effect pool.
        if (rEffect.persistence == EffectPersistence_t::Instantaneous)
            continue;
        ActiveEffect_t activeEffect;
        activeEffect.config = &rEffect;
        activeEffect.sourceId = sourceId;
        activeEffect.originBase = (rEffect.scope == EffectScope_t::ThisBase) ? pOriginBase : nullptr;
        rOut.push_back(activeEffect);
    }
}

void CollectFromBuildings(const Faction& rFaction,
                          const BuildingRegistry& rBuildingRegistry,
                          std::vector<ActiveEffect_t>& rOut)
{
    for (const auto& pBase : rFaction.GetBases())
    {
        if (!pBase)
        {
            continue;
        }

        for (const BuildingConfig_t* pBuilding : pBase->GetBuildings())
        {
            if (!pBuilding)
            {
                continue;
            }

            AppendEffects(pBuilding->effects, pBase.get(), pBuilding->id, rOut);
        }
    }

    // Key: (originBase*, grantedBuildingId). Pointer identity is intentional — two different
    // bases granting the same building must each expand independently. Pointers are always
    // valid here because rFaction owns the bases and outlives this stack frame.
    std::set<std::pair<const BaseManager*, std::string>> processedGrantedIds;
    for (size_t i = 0; i < rOut.size(); ++i)
    {
        const EffectConfig_t* pEffect = rOut[i].config;
        if (!pEffect)
        {
            continue;
        }

        const GrantBuildingEffect_t* pGrant = std::get_if<GrantBuildingEffect_t>(&pEffect->effect);
        if (!pGrant)
        {
            continue;
        }

        const BuildingConfig_t* pGranted = rBuildingRegistry.Find(pGrant->buildingId);
        if (!pGranted)
        {
            continue;
        }

        if (rOut[i].originBase != nullptr)
        {
            // ThisBase-scoped grant: expand effects once, attributed to the originating base.
            const std::pair<const BaseManager*, std::string> key = {rOut[i].originBase, pGrant->buildingId};
            if (!processedGrantedIds.count(key))
            {
                processedGrantedIds.insert(key);
                AppendEffects(pGranted->effects, rOut[i].originBase, rOut[i].sourceId + " -> " + pGranted->id, rOut);
            }
        }
        else
        {
            // AllOwnerBases / FactionGlobal grant: clone the granted building's effects once
            // per faction base so that ThisBase-scoped sub-effects are correctly attributed.
            for (const auto& pBase : rFaction.GetBases())
            {
                if (!pBase) continue;
                const std::pair<const BaseManager*, std::string> key = {pBase.get(), pGrant->buildingId};
                if (!processedGrantedIds.count(key))
                {
                    processedGrantedIds.insert(key);
                    AppendEffects(pGranted->effects, pBase.get(), rOut[i].sourceId + " -> " + pGranted->id, rOut);
                }
            }
        }
    }
}

void CollectFromSocialEngineering(const Faction& rFaction, std::vector<ActiveEffect_t>& rResult)
{
    // TODO: SocialPolicyConfig currently stores SocialScores, not EffectConfig_t.
    // Once SE selections carry EffectConfig_t effects, iterate them here.
    (void)rFaction;
    (void)rResult;
}

} // namespace

double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp>>& contributions)
{
    double addTotal = base;
    double arithmeticFactor = 1.0;
    double geometricFactor = 1.0;
    for (const auto& [amount, op] : contributions)
    {
        switch (op)
        {
            case ModifierOp::Add:                addTotal += amount; break;
            case ModifierOp::MultiplyArithmetic: arithmeticFactor += amount - 1.0; break;
            case ModifierOp::MultiplyGeometric:  geometricFactor *= amount; break;
        }
    }
    return addTotal * arithmeticFactor * geometricFactor;
}

std::vector<ActiveEffect_t> CollectActiveEffects(const Faction& rFaction,
                                                 const BuildingRegistry& rBuildingRegistry)
{
    std::vector<ActiveEffect_t> result;
    CollectFromBuildings(rFaction, rBuildingRegistry, result);
    CollectFromSocialEngineering(rFaction, result);
    return result;
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
        if (pStatModifier && pStatModifier->stat == statId)
        {
            matching.push_back(effect);
        }
    }
    return matching;
}

std::vector<ActiveEffect_t> FilterForBase(const std::vector<ActiveEffect_t>& effects, const BaseManager& rBase)
{
    std::vector<ActiveEffect_t> matching;
    for (const ActiveEffect_t& effect : effects)
    {
        if (!effect.config)
        {
            continue;
        }

        switch (effect.config->scope)
        {
            case EffectScope_t::ThisBase:
                if (effect.originBase == &rBase)
                {
                    matching.push_back(effect);
                }
                break;
            case EffectScope_t::AllOwnerBases:
            case EffectScope_t::FactionGlobal:
            case EffectScope_t::WorldGlobal:
                matching.push_back(effect);
                break;
            case EffectScope_t::ThisUnit:
            case EffectScope_t::FactionUnits:
            case EffectScope_t::ThisPop:
            case EffectScope_t::ThisTile:
                // Unit-, pop-, and tile-scoped effects never apply to base-level calculations.
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
        if (!pComp)
        {
            continue;
        }
        for (const EffectConfig_t& rEffect : pComp->effects)
        {
            ActiveEffect_t active;
            active.config = &rEffect;
            active.sourceId = pComp->id;
            result.push_back(active);
        }
    }
    return result;
}

std::vector<ActiveEffect_t> CollectPopEffects(const PopTypeConfig_t& rConfig)
{
    std::vector<ActiveEffect_t> result;
    for (const EffectConfig_t& rEffect : rConfig.effects)
    {
        ActiveEffect_t active;
        active.config = &rEffect;
        active.sourceId = rConfig.id;
        result.push_back(active);
    }
    return result;
}

std::vector<ActiveEffect_t> CollectFromPops(const PopContainer& rPops, const BaseManager& rOriginBase)
{
    std::vector<ActiveEffect_t> result;
    for (const auto& pPop : rPops.GetPops())
    {
        if (!pPop)
        {
            continue;
        }

        const std::vector<ActiveEffect_t> flatEffects =
            FilterByScope(CollectPopEffects(pPop->GetConfig()), EffectScope_t::ThisBase);

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
    for (const std::string& featureId : rTile.GetFeatureIds())
    {
        const ImprovementConfig_t* pFeature = rImprovements.Find(featureId);
        if (!pFeature)
        {
            continue;
        }

        for (const EffectConfig_t& rEffect : pFeature->effects)
        {
            ActiveEffect_t active;
            active.config = &rEffect;
            active.sourceId = pFeature->id;
            result.push_back(active);
        }
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
            rBase.AddBuilding(pGrant->buildingId);
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
