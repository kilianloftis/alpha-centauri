#include "lib/effects/ActiveEffect.h"

#include "game/Faction.h"
#include "game/buildings/BuildingConfigParser.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/units/UnitComponentConfig.h"
#include "lib/effects/BonusEffect.h"
#include <algorithm>
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

        const std::pair<const BaseManager*, std::string> key = {rOut[i].originBase, pGrant->buildingId};
        if (processedGrantedIds.count(key))
        {
            continue;
        }
        processedGrantedIds.insert(key);

        const BuildingConfig_t* pGranted = rBuildingRegistry.Find(pGrant->buildingId);
        if (!pGranted)
        {
            continue;
        }

        AppendEffects(pGranted->effects, rOut[i].originBase, rOut[i].sourceId + " -> " + pGranted->id, rOut);
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

        double amount = 0.0;
        ModifierOp op = ModifierOp::Add;

        const StatModifierEffect_t* pStatModifier = std::get_if<StatModifierEffect_t>(&active.config->effect);
        const TileYieldModifierEffect_t* pTileModifier = std::get_if<TileYieldModifierEffect_t>(&active.config->effect);
        if (pStatModifier)
        {
            amount = pStatModifier->amount;
            op = pStatModifier->op;
        }
        else if (pTileModifier)
        {
            amount = pTileModifier->amount;
            op = pTileModifier->op;
        }
        else
        {
            continue;
        }

        StatBreakdown_t::Contribution contribution;
        contribution.sourceId = active.sourceId;
        contribution.amount = amount;
        contribution.op = op;
        breakdown.contributions.push_back(contribution);
    }

    std::sort(breakdown.contributions.begin(), breakdown.contributions.end(),
              [](const StatBreakdown_t::Contribution& a, const StatBreakdown_t::Contribution& b)
              {
                  return a.sourceId < b.sourceId;
              });

    double addTotal = baseValue;
    double arithmeticFactor = 1.0;
    double geometricFactor = 1.0;

    for (const StatBreakdown_t::Contribution& contribution : breakdown.contributions)
    {
        if (contribution.op == ModifierOp::Add)
        {
            addTotal += contribution.amount;
        }
        else if (contribution.op == ModifierOp::MultiplyArithmetic)
        {
            arithmeticFactor += contribution.amount - 1.0;
        }
        else if (contribution.op == ModifierOp::MultiplyGeometric)
        {
            geometricFactor *= contribution.amount;
        }
    }

    breakdown.total = addTotal * arithmeticFactor * geometricFactor;
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
                // Unit- and pop-scoped effects never apply to base-level calculations.
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

} // namespace ac
