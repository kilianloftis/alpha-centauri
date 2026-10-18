#include "game/buildings/BuildingMutations.h"

#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingRules.h"
#include "game/effects/DeployCooldown.h"
#include "game/faction/Faction.h"
#include "lib/RandomRoll.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

namespace
{

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
}

} // namespace

void DestroyBuildingAndNotify(BaseManager& rBase, const BuildingConfig_t& rBuilding)
{
    rBase.GetBuildingManager().DestroyBuilding(rBuilding.id);
}

std::vector<BuildingId_t> DestroyRandomFacilities(BaseManager& rBase, int count, bool bExcludeHq,
                                                  bool bExcludeSecretProjects, std::mt19937& rRng)
{
    if (count <= 0)
    {
        return {};
    }

    std::vector<const BuildingConfig_t*> candidates =
        CollectDestroyableFacilities(rBase, bExcludeHq, bExcludeSecretProjects);
    if (candidates.empty())
    {
        return {};
    }

    const int toDestroy = std::min(count, static_cast<int>(candidates.size()));
    std::shuffle(candidates.begin(), candidates.end(), rRng);
    std::vector<BuildingId_t> destroyed;
    destroyed.reserve(static_cast<size_t>(toDestroy));
    for (int i = 0; i < toDestroy; ++i)
    {
        const BuildingConfig_t& rBuilding = *candidates[static_cast<size_t>(i)];
        destroyed.push_back(rBuilding.id);
        DestroyBuildingAndNotify(rBase, rBuilding);
    }
    return destroyed;
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
        pAttackerBuilding ? FindOrbitalAttackEffect(*pAttackerBuilding) : nullptr;
    const int year = rGameState.GetMissionYear();
    if (!pEffect || rAttacker.CountReadyBuildings(attackerBuildingId, year) <= 0)
    {
        return result;
    }

    // Best-effort base attribution for the deploy ledger (see Faction::DeployBuilding): the
    // faction-wide ready count above does not track which specific copy answers this attack,
    // so this picks the first base holding one. Precise per-copy tracking is Package 8.
    const BaseManager* pAttackerBase = rAttacker.FindBaseWithBuilding(attackerBuildingId);
    const BaseId_t attackerBaseId = pAttackerBase ? pAttackerBase->GetBaseId() : BaseId_t{};

    result.bAttempted = true;
    rAttacker.DeployBuilding(attackerBaseId, attackerBuildingId,
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
