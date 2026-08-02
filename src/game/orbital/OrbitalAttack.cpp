#include "game/orbital/OrbitalAttack.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/DeployCooldown.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "lib/RandomRoll.h"

#include <stdexcept>
#include <unordered_set>

namespace ac
{

namespace
{

const OrbitalAttackEffect_t* FindOrbitalAttackEffect_(const BuildingConfig_t& rBuilding)
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

void DestroyOneBuilding_(Faction& rOwner, const BuildingId_t& buildingId)
{
    BaseManager* pBase = rOwner.FindBaseWithBuilding(buildingId);
    if (!pBase)
    {
        // The caller has already confirmed the owner holds this building.
        throw std::logic_error("DestroyOneBuilding_: faction owns no base holding building '"
                               + buildingId + "'");
    }
    pBase->GetBuildingManager().DestroyBuilding(buildingId);
    rOwner.NotifyBuildingDestroyed(buildingId);
}

} // namespace

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
            const OrbitalAttackEffect_t* pEffect = FindOrbitalAttackEffect_(*pBuilding);
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

OrbitalAttackResult_t TryAttackSatellite(GameState& rGameState,
                                         Faction& rAttacker,
                                         Faction& rDefender,
                                         const BuildingId_t& attackerBuildingId,
                                         const BuildingId_t& targetOrbitalBuildingId,
                                         std::mt19937& rRng)
{
    OrbitalAttackResult_t result;
    result.attackerBuildingId = attackerBuildingId;
    result.targetBuildingId = targetOrbitalBuildingId;

    if (&rAttacker == &rDefender)
    {
        throw std::logic_error("TryAttackSatellite: a faction cannot ASAT its own orbitals");
    }

    const BuildingConfig_t* pTarget = rDefender.FindOwnedBuildingConfig(targetOrbitalBuildingId);
    if (!pTarget || !pTarget->orbital)
    {
        return result;
    }

    const BuildingConfig_t* pAttackerBuilding =
        rAttacker.FindOwnedBuildingConfig(attackerBuildingId);
    const OrbitalAttackEffect_t* pEffect =
        pAttackerBuilding ? FindOrbitalAttackEffect_(*pAttackerBuilding) : nullptr;
    const int year = rGameState.GetMissionYear();
    if (!pEffect || rAttacker.CountReadyBuildings(attackerBuildingId, year) <= 0)
    {
        return result;
    }

    result.bAttempted = true;
    rAttacker.DeployBuilding(attackerBuildingId,
                             ReadyYearAfterDeploy(year, pEffect->cooldownTurns));

    result.bHit = RollPercent(pEffect->chance, rRng);
    if (result.bHit)
    {
        DestroyOneBuilding_(rDefender, targetOrbitalBuildingId);
    }
    else if (RollPercent(pEffect->chanceOfDestructionOnFail, rRng))
    {
        DestroyOneBuilding_(rAttacker, attackerBuildingId);
        result.bAttackerDestroyed = true;
    }
    return result;
}

} // namespace ac
