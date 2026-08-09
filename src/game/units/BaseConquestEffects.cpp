#include "game/units/BaseConquestEffects.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Military.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/MovementRules.h"
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

const BaseConquestConfig_t& RequireBaseConquestConfig_(const GameDataContext& rData)
{
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

std::vector<const BuildingConfig_t*> DestroyableFacilities_(const BaseManager& rBase)
{
    std::vector<const BuildingConfig_t*> candidates;
    for (const BuildingConfig_t* pBuilding : rBase.GetBuildingManager().GetBuildings())
    {
        if (pBuilding && !pBuilding->bIsSecretProject)
        {
            candidates.push_back(pBuilding);
        }
    }
    return candidates;
}

int DestroyRandomFacilities_(BaseManager& rBase, const BaseConquestConfig_t& rConfig,
                             std::mt19937& rRng)
{
    std::vector<const BuildingConfig_t*> candidates = DestroyableFacilities_(rBase);
    if (candidates.empty())
    {
        return 0;
    }

    // Both bounds clamp to what the base actually has, so the range is never inverted.
    const int eligible = static_cast<int>(candidates.size());
    const int minCount = std::min(rConfig.captureFacilitiesDestroyedMin, eligible);
    const int maxCount =
        std::min(std::max(minCount, eligible * rConfig.captureFacilitiesDestroyedMaxPercent / 100),
                 eligible);

    std::uniform_int_distribution<int> countDist(minCount, maxCount);
    const int toDestroy = countDist(rRng);

    std::shuffle(candidates.begin(), candidates.end(), rRng);
    Faction& rOwner = rBase.GetFaction();
    const BaseId_t baseId = rBase.GetBaseId();
    for (int i = 0; i < toDestroy; ++i)
    {
        const BuildingId_t id = candidates[static_cast<size_t>(i)]->id;
        rBase.GetBuildingManager().DestroyBuilding(id);
        rOwner.NotifyBuildingDestroyed(baseId, id);
    }
    return toDestroy;
}

bool MaybeRazeBase_(BaseManager& rBase, GameState& rGameState, BaseConquestResult_t& rResult)
{
    if (!rBase.GetPopulation().IsDestroyed())
    {
        return false;
    }

    rGameState.RazeBase(rBase);
    rResult.bBaseRazed = true;
    rResult.outcome = BaseConquestOutcome_t::Razed;
    return true;
}

const UnitDesign* EnsureEscapePodDesign_(Faction& rFaction, const GameDataContext& rDataContext,
                                         const EscapeColonyPodConfig_t& rPodConfig)
{
    // No configured pod is a legitimate "this ruleset has no escape pods" — nothing to build.
    if (rPodConfig.componentIds.empty())
    {
        return nullptr;
    }
    // A configured pod that cannot be assembled is a config error, not a silent no-op: it used
    // to return null and spawn nothing, so a cross-species capture stripped population and
    // produced no pods — the player lost the pops the rule says they escape with, silently.
    //
    // These throws should be unreachable: LoadGameData validates every componentIds entry
    // against the registry (and ThrowIfIncomplete guarantees the registry itself), so a bad
    // config fails at startup naming the file rather than here, mid-capture, after facilities
    // have already been destroyed. Kept as assertions for a context assembled by hand.
    if (!rDataContext.unitComponentRegistry)
    {
        throw std::runtime_error(
            "EnsureEscapePodDesign_: escape colony pods are configured but no unit component "
            "registry is available to build them");
    }

    std::vector<UnitSlotConfig_t> slots;
    std::unordered_map<std::string, const UnitComponentConfig_t*> assigned;
    int slotIndex = 0;
    for (const std::string& rId : rPodConfig.componentIds)
    {
        const UnitComponentConfig_t* pComponent = rDataContext.unitComponentRegistry->Find(rId);
        if (!pComponent)
        {
            throw std::runtime_error(
                "EnsureEscapePodDesign_: escape colony pod component '" + rId
                + "' is not in the unit component registry");
        }
        UnitSlotConfig_t slot;
        slot.id = "escape_slot_" + std::to_string(slotIndex++);
        slot.displayName = slot.id;
        slot.componentType = pComponent->type;
        slot.required = true;
        assigned[slot.id] = pComponent;
        slots.push_back(slot);
    }

    auto pDesign = std::make_unique<UnitDesign>(slots, assigned);
    const std::string designId = pDesign->GetId();
    if (const UnitDesign* pExisting = rFaction.GetMilitary().GetDesign(designId))
    {
        return pExisting;
    }
    rFaction.GetMilitary().AddDesign(std::move(pDesign));
    return rFaction.GetMilitary().GetDesign(designId);
}

int SpawnEscapePods_(Faction& rFleeingFaction, const Tile& rOrigin, int podCount,
                     GameState& rGameState, const GameDataContext& rDataContext,
                     const EscapeColonyPodConfig_t& rPodConfig, std::mt19937& rRng)
{
    if (podCount <= 0)
    {
        return 0;
    }

    const UnitDesign* pDesign =
        EnsureEscapePodDesign_(rFleeingFaction, rDataContext, rPodConfig);
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

// Human ↔ Progenitor capture: the base keeps one pop, half the displaced colonists flee as
// escape pods. Reports both counts through rResult, which owns every conquest tally.
void ApplySpeciesClashPopulation_(BaseManager& rBase, GameState& rGameState,
                                  const GameDataContext& rDataContext,
                                  const BaseConquestConfig_t& rConfig, std::mt19937& rRng,
                                  BaseConquestResult_t& rResult)
{
    Faction& rOwner = rBase.GetFaction();
    const int size = rBase.GetPopulation().GetSize();
    if (size <= 1)
    {
        return;
    }

    const int removed = size - 1;
    const int flee = removed / 2;

    rResult.escapePodsSpawned = SpawnEscapePods_(
        rOwner, rBase.GetTile(), flee, rGameState, rDataContext, rConfig.escapeColonyPod, rRng);

    // Leave population 1 in the base; flee pods are separate units on adjacent tiles.
    rResult.populationLost = RemovePopulation_(rBase, removed);
}

void RepairCapturer_(Unit& rCapturer)
{
    if (ResolveFlag(rCapturer, RuleFlagId_t::NoConquestRepair))
    {
        return;
    }
    rCapturer.SetCurrentHp(ResolveStat(rCapturer, StatId_t::HitPoints));
}

BaseConquestResult_t ApplyLastDefenderCasualty_(BaseManager& rBase, GameState& rGameState,
                                                const BaseConquestConfig_t& rConfig)
{
    BaseConquestResult_t result;
    result.outcome = BaseConquestOutcome_t::LastDefenderCasualty;

    if (!ResolveFlag(rBase, RuleFlagId_t::PreventsConquestPopLoss))
    {
        result.populationLost = RemovePopulation_(rBase, rConfig.lastDefenderPopLoss);
    }

    MaybeRazeBase_(rBase, rGameState, result);
    return result;
}

BaseConquestResult_t ApplyNativeRaid_(Unit& rNative, BaseManager& rBase, GameState& rGameState,
                                      std::mt19937& rRng)
{
    BaseConquestResult_t result;
    result.outcome = BaseConquestOutcome_t::NativeRaid;

    std::vector<const BuildingConfig_t*> facilities = DestroyableFacilities_(rBase);
    std::uniform_int_distribution<int> coin(0, 1);
    const bool bDestroyFacility = !facilities.empty() && (rBase.GetPopulation().GetSize() <= 0
                                                          || coin(rRng) == 0);

    if (bDestroyFacility)
    {
        std::uniform_int_distribution<size_t> pick(0, facilities.size() - 1);
        const BuildingId_t id = facilities[pick(rRng)]->id;
        rBase.GetBuildingManager().DestroyBuilding(id);
        rBase.GetFaction().NotifyBuildingDestroyed(rBase.GetBaseId(), id);
        result.facilitiesDestroyed = 1;
    }
    else if (rBase.GetPopulation().GetSize() > 0)
    {
        result.populationLost = RemovePopulation_(rBase, 1);
    }

    rNative.GetFaction().GetUnitManager().DestroyUnit(rNative);
    result.bActorDestroyed = true;

    MaybeRazeBase_(rBase, rGameState, result);
    return result;
}

BaseConquestResult_t ApplyCapture_(Unit& rCapturer, BaseManager& rBase, GameState& rGameState,
                                   const GameDataContext& rDataContext, std::mt19937& rRng)
{
    BaseConquestResult_t result;
    result.outcome = BaseConquestOutcome_t::Captured;

    const BaseConquestConfig_t& rConfig = RequireBaseConquestConfig_(rDataContext);
    Faction& rOldOwner = rBase.GetFaction();
    Faction& rNewOwner = rCapturer.GetFaction();
    const BaseId_t baseId = rBase.GetBaseId();

    result.facilitiesDestroyed = DestroyRandomFacilities_(rBase, rConfig, rRng);

    if (IsCrossSpeciesConquest(rNewOwner.GetDefinition().identity.species,
                               rOldOwner.GetDefinition().identity.species))
    {
        ApplySpeciesClashPopulation_(rBase, rGameState, rDataContext, rConfig, rRng, result);
    }
    else
    {
        result.populationLost = RemovePopulation_(rBase, rConfig.capturePopLoss);
    }

    if (MaybeRazeBase_(rBase, rGameState, result))
    {
        return result;
    }

    rOldOwner.TransferBaseTo(baseId, rNewOwner);
    RepairCapturer_(rCapturer);
    return result;
}

} // namespace

BaseConquestResult_t ResolvePostCombatBaseConquest(Unit& rAttacker,
                                                   const Tile& rDefenderTile,
                                                   GameState& rGameState,
                                                   const GameDataContext& rDataContext,
                                                   std::mt19937& rRng)
{
    BaseManager* pBase = rGameState.FindBaseAt(rDefenderTile.GetX(), rDefenderTile.GetY());
    if (!pBase)
    {
        return {};
    }
    if (HasBaseGarrison(*pBase, rGameState.GetWorldMap()))
    {
        return {};
    }

    // Capture requires stepping onto the tile; combat only applies last-defender
    // casualties (and native raids from the adjacent attacker).
    const BaseConquestConfig_t& rConfig = RequireBaseConquestConfig_(rDataContext);
    BaseConquestResult_t result = ApplyLastDefenderCasualty_(*pBase, rGameState, rConfig);
    if (result.bBaseRazed)
    {
        return result;
    }

    if (IsNativeLifeFaction(rAttacker.GetFaction().GetDefinition().identity.species))
    {
        BaseConquestResult_t raid = ApplyNativeRaid_(rAttacker, *pBase, rGameState, rRng);
        raid.populationLost += result.populationLost;
        return raid;
    }

    return result;
}

BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover,
                                              GameState& rGameState,
                                              const GameDataContext& rDataContext,
                                              std::mt19937& rRng)
{
    BaseManager* pBase =
        rGameState.FindBaseAt(rMover.GetTile().GetX(), rMover.GetTile().GetY());
    if (!pBase)
    {
        return {};
    }
    if (pBase->GetFactionId() == rMover.GetFaction().GetFactionId())
    {
        return {};
    }
    if (HasBaseGarrison(*pBase, rGameState.GetWorldMap()))
    {
        return {};
    }

    if (IsNativeLifeFaction(rMover.GetFaction().GetDefinition().identity.species))
    {
        return ApplyNativeRaid_(rMover, *pBase, rGameState, rRng);
    }

    if (CanCaptureBase(rMover))
    {
        return ApplyCapture_(rMover, *pBase, rGameState, rDataContext, rRng);
    }

    return {};
}

} // namespace ac
