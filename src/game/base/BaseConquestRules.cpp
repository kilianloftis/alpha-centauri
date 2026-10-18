#include "game/base/BaseConquestRules.h"

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

namespace ac
{

bool HasBaseGarrison(const BaseManager& rBase, const WorldMap& rWorldMap)
{
    const FactionId_t ownerId = rBase.GetFactionId();
    for (const Unit* pUnit : rWorldMap.GetAllUnitsOnTile(rBase.GetTile()))
    {
        // Occupants and cargo both count: cargo in a base is still a garrison (it survives
        // its carrier being sunk in port via SurvivesCarrierLoss and holds the base against
        // capture). Embarked-in-base cargo is also eligible as a combat defender — see
        // AttackRules::FindVisibleHostileOnTile — with the carrier preferred when both are
        // present.
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

// Every base_conquest stat is Additive: base_conquest.json Adds the baseline, and facilities
// and difficulty stack on top.
int ResolveBaseConquestStat_(const BaseManager& rBase, StatId_t stat)
{
    const double resolved = ResolveBaseStat(rBase.GetBaseEffects(), stat, SeedFor(stat));
    return std::max(0, FinalizeResolvedStat(resolved));
}

} // namespace

std::optional<ConquestKind_t> ConquestOnEntry(const Unit& rMover, const BaseManager& rBase,
                                              const WorldMap& rWorldMap)
{
    if (rBase.GetFactionId() == rMover.GetFaction().GetFactionId()
        || HasBaseGarrison(rBase, rWorldMap))
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
                                                         const BaseManager& rBase,
                                                         const WorldMap& rWorldMap)
{
    if (HasBaseGarrison(rBase, rWorldMap))
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
        CollectDestroyableFacilities(rBase, /*excludeHq=*/false, /*excludeSecretProjects=*/true);
    if (!outcome.facilityCandidates.empty())
    {
        // Both bounds clamp to what the base has, so no modifier can invert the range.
        const int eligible = static_cast<int>(outcome.facilityCandidates.size());
        const int minStat =
            ResolveBaseConquestStat_(rBase, StatId_t::CaptureFacilitiesDestroyedMin);
        const int maxStat =
            ResolveBaseConquestStat_(rBase, StatId_t::CaptureFacilitiesDestroyedMaxPercent);
        const int maxPercent = std::clamp(maxStat, 0, 100);
        outcome.minFacilitiesDestroyed = std::clamp(minStat, 0, eligible);
        const int maxCount = std::max(outcome.minFacilitiesDestroyed, eligible * maxPercent / 100);
        outcome.maxFacilitiesDestroyed = std::min(maxCount, eligible);
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
    NativeRaidPlan_t plan;
    plan.facilityCandidates =
        CollectDestroyableFacilities(rBase, /*excludeHq=*/false, /*excludeSecretProjects=*/true);
    const bool bHasPop = rBase.GetPopulation().GetSize() > 0;
    if (!plan.facilityCandidates.empty())
    {
        plan.facilityChancePercent = bHasPop ? 50 : 100;
    }
    plan.bPopCanBeLost = bHasPop;
    return plan;
}

} // namespace ac
