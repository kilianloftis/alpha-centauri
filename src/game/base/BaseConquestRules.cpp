#include "game/base/BaseConquestRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseConquestCalculator.h"
#include "game/base/BaseConquestConfig.h"
#include "game/base/BaseRules.h"
#include "game/economy/EconomyManager.h"
#include "game/map/MapUtils.h"
#include "game/map/RegionCache.h"
#include "game/faction/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionConfig.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingRules.h"
#include "game/population/PopulationManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

namespace
{

// HQ drops on transfer; secret projects and Pressure Dome are outside capture_base's 1/3 loop.
constexpr FacilityExclusions_t k_CaptureFacilityExclusions{.excludeHq = true,
                                                           .excludeSecretProjects = true,
                                                           .excludePressureDome = true};
// A native raid may take the Headquarters but never a secret project.
constexpr FacilityExclusions_t k_RaidFacilityExclusions{.excludeHq = false,
                                                        .excludeSecretProjects = true};

const HeadquartersEscapeConfig_t& EscapeConfig_()
{
    return CurrentGameState().GetGameData().baseConquestConfig->headquartersEscape;
}

bool CanAffordEscape_(const Faction& rOwner)
{
    return rOwner.GetEconomy().CanAfford(EscapeConfig_().cost);
}

// rCandidate against its owner's other bases, rLost excluded.
EscapeScoreInputs_t EscapeScoreInputs_(const BaseManager& rCandidate, const BaseManager& rLost)
{
    const WorldMap& rMap = CurrentGameState().GetWorldMap();
    const RegionCache& rRegions = rMap.GetRegions();
    const int region = rRegions.RegionOf(rCandidate.GetTile());
    EscapeScoreInputs_t inputs;
    for (const BaseManager& rOther : rCandidate.GetFaction().Bases())
    {
        if (&rOther == &rCandidate || &rOther == &rLost)
        {
            continue;
        }
        const int distance =
            TabletopDiagonalDistance(rCandidate.GetTile(), rOther.GetTile(), rMap.GetWidth());
        if (rRegions.RegionOf(rOther.GetTile()) == region)
        {
            inputs.sameRegionDistance += distance;
        }
        else
        {
            inputs.otherRegionDistance += distance;
        }
    }
    inputs.regionBases = RegionBaseCount(rCandidate.GetFaction(), region);
    inputs.population = rCandidate.GetPopulation().GetSize();
    return inputs;
}

} // namespace

bool HasBaseGarrison(const BaseManager& rBase)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const FactionId_t ownerId = rBase.GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetAllUnitsOnTile(rBase.GetTile()))
    {
        // Cargo in a base counts too: it survives its carrier sinking in port and holds the base.
        if (pUnit && pUnit->GetFaction().GetFactionId() == ownerId)
        {
            return true;
        }
    }
    return false;
}

bool CanCaptureBase(const Unit& rCapturer)
{
    return !ResolveFlag(rCapturer, RuleFlagId_t::CannotCaptureBases);
}

bool IsCrossSpeciesConquest(FactionSpecies_t attacker, FactionSpecies_t defender)
{
    return (attacker == FactionSpecies_t::Human && defender == FactionSpecies_t::Progenitor)
        || (attacker == FactionSpecies_t::Progenitor && defender == FactionSpecies_t::Human);
}

namespace
{

bool IsNativeRaider_(const Unit& rUnit)
{
    return IsNativeLifeFaction(rUnit.GetFaction().GetDefinition().identity.species);
}

// Additive conquest stats: base_conquest.json Adds the baseline; facilities and difficulty stack.
int ResolveBaseConquestStat_(const BaseManager& rBase, StatId_t stat)
{
    const double resolved = ResolveBaseStat(rBase.GetBaseEffects(), stat, SeedFor(stat), nullptr);
    return std::max(0, FinalizeResolvedStat(resolved));
}

int ResolveCaptureFacilitiesToDestroy_(const BaseManager& rBase, int eligible)
{
    const double seed = static_cast<double>(eligible);
    const double resolved = ResolveBaseStat(rBase.GetBaseEffects(),
                                            StatId_t::CaptureFacilitiesDestroyed, seed, nullptr);
    return std::clamp(FinalizeResolvedStat(resolved), 0, eligible);
}

} // namespace

std::optional<ConquestKind_t> ConquestOnEntry(const Unit& rMover, const BaseManager& rBase)
{
    if (rBase.GetFactionId() == rMover.GetFaction().GetFactionId() || HasBaseGarrison(rBase))
    {
        return std::nullopt;
    }
    if (IsNativeRaider_(rMover))
    {
        return ConquestKind_t::NativeRaid;
    }
    if (CanCaptureBase(rMover))
    {
        return ConquestKind_t::Capture;
    }
    return std::nullopt;
}

std::optional<ConquestKind_t> ConquestOnLastDefenderKill(const Unit& rAttacker,
                                                         const BaseManager& rBase)
{
    if (HasBaseGarrison(rBase))
    {
        return std::nullopt;
    }
    return IsNativeRaider_(rAttacker) ? ConquestKind_t::NativeRaid
                                      : ConquestKind_t::LastDefenderCasualty;
}

int LastDefenderPopLoss(const BaseManager& rBase)
{
    return ResolveBaseConquestStat_(rBase, StatId_t::LastDefenderPopLoss);
}

CaptureOutcome_t PlanCapture(const Unit& rCapturer, const BaseManager& rBase)
{
    CaptureOutcome_t outcome;
    const FactionSpecies_t capturer = rCapturer.GetFaction().GetDefinition().identity.species;
    const FactionSpecies_t owner = rBase.GetFaction().GetDefinition().identity.species;
    const bool bCrossSpecies = IsCrossSpeciesConquest(capturer, owner);

    outcome.facilityCandidates =
        CollectDestroyableFacilities(rBase, k_CaptureFacilityExclusions);
    // Liberation: capturer is the former owner still being assimilated.
    const AssimilationState& rWindow =
        rBase.GetPopulation().GetAssimilation().OccupierWindow();
    const bool bLiberation = rWindow.IsActive()
        && rWindow.formerOwner == rCapturer.GetFaction().GetFactionId();
    outcome.bRollCaptureFacilityLoss = !bLiberation;
    if (outcome.bRollCaptureFacilityLoss && !outcome.facilityCandidates.empty())
    {
        const int eligible = static_cast<int>(outcome.facilityCandidates.size());
        outcome.facilitiesToDestroy = ResolveCaptureFacilitiesToDestroy_(rBase, eligible);
    }

    const int size = rBase.GetPopulation().GetSize();
    if (!bCrossSpecies)
    {
        outcome.popLoss = ResolveBaseConquestStat_(rBase, StatId_t::CapturePopLoss);
    }
    else if (size > 1)
    {
        outcome.popLoss = size - 1;
        outcome.escapePods = outcome.popLoss / 2;
    }
    outcome.bRepairsCapturer = !ResolveFlag(rCapturer, RuleFlagId_t::NoConquestRepair);
    return outcome;
}

NativeRaidPlan_t PlanNativeRaid(const BaseManager& rBase)
{
    const BaseConquestConfig_t* pConfig = CurrentGameState().GetGameData().baseConquestConfig.get();
    if (!pConfig)
    {
        throw std::logic_error("PlanNativeRaid: baseConquestConfig is null");
    }
    NativeRaidPlan_t plan;
    plan.popLoss = pConfig->nativeRaid.popLoss;
    plan.facilityCandidates =
        CollectDestroyableFacilities(rBase, k_RaidFacilityExclusions);
    if (!plan.facilityCandidates.empty())
    {
        plan.facilityChancePercent = pConfig->nativeRaid.facilityChancePercent;
    }
    return plan;
}

std::optional<BaseId_t> HeadquartersEscapeDestination(const BaseManager& rLost)
{
    const Faction& rOwner = rLost.GetFaction();
    if (!rLost.IsHeadquarters() || !CanAffordEscape_(rOwner))
    {
        return std::nullopt;
    }
    const BaseConquestCalculator& rCalculator =
        *CurrentGameState().GetGameData().baseConquestCalculator;
    std::optional<BaseId_t> best;
    int bestScore = 0;
    for (const BaseManager& rCandidate : rOwner.Bases())
    {
        if (&rCandidate == &rLost)
        {
            continue;
        }
        const EscapeScoreInputs_t inputs = EscapeScoreInputs_(rCandidate, rLost);
        const int score = rCalculator.HeadquartersEscapeScore(inputs);
        if (!best || score < bestScore)
        {
            best = rCandidate.GetBaseId();
            bestScore = score;
        }
    }
    return best;
}

bool MayEvacuateHeadquarters(const Faction& rOwner, const BaseManager& rDestination)
{
    return rOwner.GetHeadquarters() == nullptr && &rDestination.GetFaction() == &rOwner
        && CanAffordEscape_(rOwner);
}

} // namespace ac
