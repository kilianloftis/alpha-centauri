#include "game/base/BaseConquestMutations.h"

#include "game/units/AdHocDesign.h"

#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/units/Military.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingMutations.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/base/BaseConquestRules.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ac
{

namespace
{

const BaseConquestConfig_t& RequireBaseConquestConfig_(const GameState& rGameState)
{
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.baseConquestConfig)
    {
        throw std::runtime_error("GameDataContext: baseConquestConfig is null");
    }
    return *rData.baseConquestConfig;
}

int RemovePopulation_(BaseManager& rBase, int amount)
{
    int removed = 0;
    for (int i = 0; i < amount && rBase.GetPopulation().GetSize() > 0; ++i)
    {
        rBase.GetPopulation().RemovePop();
        ++removed;
    }
    return removed;
}

// Report whether the pop losses just applied took the base out of the game. The raze itself
// already happened, synchronously, in the pop-loss handler that RemovePopulation_ tripped.
bool NoteIfAlreadyRazed_(BaseManager& rBase, BaseConquestResult_t& rResult)
{
    if (!rBase.IsRazed())
    {
        return false;
    }

    rResult.bBaseRazed = true;
    rResult.outcome = BaseConquestOutcome_t::Razed;
    return true;
}

int SpawnEscapePods_(Faction& rFleeingFaction, const Tile& rOrigin, int podCount,
                     GameState& rGameState, const EscapeColonyPodConfig_t& rPodConfig,
                     std::mt19937& rRng)
{
    if (podCount <= 0)
    {
        return 0;
    }

    const UnitDesign* pDesign =
        EnsureAdHocDesign(rFleeingFaction, *rGameState.GetGameData().unitComponentRegistry,
                          rPodConfig.componentIds, "escape");
    if (!pDesign)
    {
        return 0;
    }

    WorldMap& rMap = rGameState.GetWorldMap();
    UnitPositionIndex& rPositions = rMap.GetUnitPositions();
    std::vector<Tile*> candidates;
    ForEachTileInChebyshevRadius(rOrigin, rMap, 1, /*includeOrigin=*/false,
        [&](Tile* pTile, int /*distance*/)
        {
            if (pTile && CanPlaceUnitOnTile(*pTile, rPositions))
            {
                candidates.push_back(pTile);
            }
        });
    if (candidates.empty())
    {
        return 0;
    }

    std::shuffle(candidates.begin(), candidates.end(), rRng);
    const int spawnCount = std::min(podCount, static_cast<int>(candidates.size()));
    for (int i = 0; i < spawnCount; ++i)
    {
        rFleeingFaction.GetUnitManager().CreateUnit(
            rGameState.AllocateUnitId(), *pDesign, rPositions, *candidates[static_cast<size_t>(i)]);
    }
    return spawnCount;
}

void DestroyCaptureFacilities_(BaseManager& rBase, const CaptureOutcome_t& rOutcome,
                               std::mt19937& rRng, BaseConquestResult_t& rResult)
{
    if (rOutcome.facilityCandidates.empty())
    {
        return;
    }
    std::uniform_int_distribution<int> countDist(rOutcome.minFacilitiesDestroyed,
                                                 rOutcome.maxFacilitiesDestroyed);
    const int toDestroy = countDist(rRng);
    std::vector<const BuildingConfig_t*> candidates = rOutcome.facilityCandidates;
    std::shuffle(candidates.begin(), candidates.end(), rRng);
    for (int i = 0; i < toDestroy; ++i)
    {
        DestroyBuildingAndNotify(rBase, *candidates[static_cast<size_t>(i)]);
    }
    rResult.facilitiesDestroyed = toDestroy;
}

BaseConquestResult_t ApplyLastDefenderCasualty_(BaseManager& rBase)
{
    BaseConquestResult_t result;
    result.outcome = BaseConquestOutcome_t::LastDefenderCasualty;
    result.populationLost = RemovePopulation_(rBase, LastDefenderPopLoss(rBase));
    NoteIfAlreadyRazed_(rBase, result);
    return result;
}

BaseConquestResult_t ApplyNativeRaid_(Unit& rNative, BaseManager& rBase, std::mt19937& rRng)
{
    BaseConquestResult_t result;
    result.outcome = BaseConquestOutcome_t::NativeRaid;

    const NativeRaidPlan_t plan = PlanNativeRaid(rBase);
    bool bDestroyFacility = plan.facilityChancePercent == 100;
    if (plan.facilityChancePercent == 50)
    {
        std::uniform_int_distribution<int> coin(0, 1);
        bDestroyFacility = coin(rRng) == 0;
    }

    if (bDestroyFacility)
    {
        std::uniform_int_distribution<size_t> pick(0, plan.facilityCandidates.size() - 1);
        DestroyBuildingAndNotify(rBase, *plan.facilityCandidates[pick(rRng)]);
        result.facilitiesDestroyed = 1;
    }
    else if (plan.bPopCanBeLost)
    {
        result.populationLost = RemovePopulation_(rBase, 1);
    }

    rNative.GetFaction().GetUnitManager().DestroyUnit(rNative);
    result.bActorDestroyed = true;

    NoteIfAlreadyRazed_(rBase, result);
    return result;
}

BaseConquestResult_t ApplyCapture_(Unit& rCapturer, BaseManager& rBase, GameState& rGameState,
                                   std::mt19937& rRng)
{
    BaseConquestResult_t result;
    result.outcome = BaseConquestOutcome_t::Captured;

    const BaseConquestConfig_t& rConfig = RequireBaseConquestConfig_(rGameState);
    Faction& rOldOwner = rBase.GetFaction();
    Faction& rNewOwner = rCapturer.GetFaction();
    const BaseId_t baseId = rBase.GetBaseId();
    const CaptureOutcome_t outcome = PlanCapture(rCapturer, rBase);

    DestroyCaptureFacilities_(rBase, outcome, rRng, result);
    // Pods first, while the base still has the pops they flee from.
    result.escapePodsSpawned = SpawnEscapePods_(rOldOwner, rBase.GetTile(), outcome.escapePods,
                                                rGameState, rConfig.escapeColonyPod, rRng);
    result.populationLost = RemovePopulation_(rBase, outcome.popLoss);

    if (NoteIfAlreadyRazed_(rBase, result))
    {
        return result;
    }

    rBase.GetPopulation().NotifyCaptured(rOldOwner.GetFactionId(), rNewOwner.GetFactionId());
    rOldOwner.TransferBaseTo(baseId, rNewOwner);
    if (outcome.bRepairsCapturer)
    {
        rCapturer.SetCurrentHp(ResolveStat(rCapturer, StatId_t::HitPoints));
    }
    return result;
}

} // namespace

BaseConquestResult_t ResolvePostCombatBaseConquest(Unit& rAttacker,
                                                   const Tile& rDefenderTile,
                                                   GameState& rGameState,
                                                   std::mt19937& rRng)
{
    BaseManager* pBase = rGameState.FindBaseAt(rDefenderTile.GetX(), rDefenderTile.GetY());
    if (!pBase)
    {
        return {};
    }
    const std::optional<ConquestKind_t> kind =
        ConquestOnLastDefenderKill(rAttacker, *pBase, rGameState.GetWorldMap());
    if (!kind)
    {
        return {};
    }

    // Capture requires stepping onto the tile; combat only applies last-defender
    // casualties (and native raids from the adjacent attacker).
    BaseConquestResult_t result = ApplyLastDefenderCasualty_(*pBase);
    if (result.bBaseRazed || *kind != ConquestKind_t::NativeRaid)
    {
        return result;
    }
    BaseConquestResult_t raid = ApplyNativeRaid_(rAttacker, *pBase, rRng);
    raid.populationLost += result.populationLost;
    return raid;
}

BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover,
                                              GameState& rGameState,
                                              std::mt19937& rRng)
{
    BaseManager* pBase =
        rGameState.FindBaseAt(rMover.GetTile().GetX(), rMover.GetTile().GetY());
    if (!pBase)
    {
        return {};
    }
    const std::optional<ConquestKind_t> kind =
        ConquestOnEntry(rMover, *pBase, rGameState.GetWorldMap());
    if (kind == ConquestKind_t::NativeRaid)
    {
        return ApplyNativeRaid_(rMover, *pBase, rRng);
    }
    if (kind == ConquestKind_t::Capture)
    {
        return ApplyCapture_(rMover, *pBase, rGameState, rRng);
    }
    return {};
}

} // namespace ac
