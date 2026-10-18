#include "game/buildings/BuildingRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"

#include <stdexcept>
#include <string>
#include <unordered_set>

namespace ac
{

namespace
{

bool BuildingIsHeadquarters_(const BuildingConfig_t& rBuilding)
{
    for (const EffectConfig_t& rEffect : rBuilding.effects)
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.effect);
        if (pFlag && pFlag->flag == RuleFlagId_t::Headquarters)
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool MayHoldBuilding(const BuildingManager& rBuildings, const BuildingConfig_t& rBuilding)
{
    if (!rBuilding.allowMultiple && rBuildings.HasBuilding(rBuilding.id))
    {
        return false;
    }
    if (!rBuilding.bIsSecretProject)
    {
        return true;
    }
    return !IsSecretProjectUnavailable(rBuilding.id);
}

bool IsSecretProjectUnavailable(const BuildingId_t& buildingId)
{
    // TODO(difficulty): when rules.ai_secret_projects_require_human_prereq, AI may not start
    // an SP until a human faction has the prerequisite tech.
    return CurrentGameState().IsSecretProjectDestroyed(buildingId)
           || IsSecretProjectOwned(buildingId);
}

bool IsSecretProjectOwned(const BuildingId_t& buildingId)
{
    for (const Faction& rFaction : CurrentGameState().Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            if (rBase.GetBuildingManager().HasBuilding(buildingId))
            {
                return true;
            }
        }
    }
    return false;
}

std::vector<const BuildingConfig_t*> CollectDestroyableFacilities(const BaseManager& rBase,
                                                                  bool bExcludeHq,
                                                                  bool bExcludeSecretProjects)
{
    std::vector<const BuildingConfig_t*> candidates;
    for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
    {
        if (!pBuilding)
        {
            continue;
        }
        if (bExcludeSecretProjects && pBuilding->bIsSecretProject)
        {
            continue;
        }
        if (bExcludeHq && BuildingIsHeadquarters_(*pBuilding))
        {
            continue;
        }
        candidates.push_back(pBuilding);
    }
    return candidates;
}

const OrbitalAttackEffect_t* FindOrbitalAttackEffect(const BuildingConfig_t& rBuilding)
{
    for (const EffectConfig_t& rEffect : rBuilding.effects)
    {
        if (const auto* pAttack = std::get_if<OrbitalAttackEffect_t>(&rEffect.effect))
        {
            return pAttack;
        }
    }
    return nullptr;
}

const OrbitalAttackEffect_t* ReadyOrbitalAttack(const Faction& rAttacker,
                                                const Faction& rDefender,
                                                const BuildingId_t& attackerBuildingId,
                                                const BuildingId_t& targetBuildingId,
                                                int missionYear)
{
    const BuildingConfig_t* pTarget = rDefender.FindOwnedBuildingConfig(targetBuildingId);
    if (pTarget == nullptr || !pTarget->orbital)
    {
        return nullptr;
    }
    const BuildingConfig_t* pBuilding = rAttacker.FindOwnedBuildingConfig(attackerBuildingId);
    if (pBuilding == nullptr)
    {
        return nullptr;
    }
    const OrbitalAttackEffect_t* pEffect = FindOrbitalAttackEffect(*pBuilding);
    if (pEffect == nullptr || rAttacker.CountReadyBuildings(attackerBuildingId, missionYear) <= 0)
    {
        return nullptr;
    }
    return pEffect;
}

std::vector<OrbitalAttackerOption_t> ListReadyOrbitalAttackers(const Faction& rFaction,
                                                               int missionYear)
{
    std::vector<OrbitalAttackerOption_t> options;
    std::unordered_set<BuildingId_t> seen;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
        {
            if (!pBuilding || !seen.insert(pBuilding->id).second)
            {
                continue;
            }
            const OrbitalAttackEffect_t* pEffect = FindOrbitalAttackEffect(*pBuilding);
            if (!pEffect)
            {
                continue;
            }
            const int ready = rFaction.CountReadyBuildings(pBuilding->id, missionYear);
            if (ready <= 0)
            {
                continue;
            }
            OrbitalAttackerOption_t option;
            option.buildingId = pBuilding->id;
            option.pConfig = pBuilding;
            option.readyCount = ready;
            option.chance = pEffect->chance;
            option.cooldownTurns = pEffect->cooldownTurns;
            options.push_back(option);
        }
    }
    return options;
}

} // namespace ac
