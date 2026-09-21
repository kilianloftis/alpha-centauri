#include "game/units/ProbeActionEffects.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BuildingDestruction.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/production/ProductionManager.h"
#include "game/research/TechConfigParser.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/Unit.h"

#include <algorithm>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

namespace ac
{

namespace
{

// Every base mission resolves its config effects the same way: the acting faction is the
// subject (it gains the infiltration), the target's base is what the effects act on, and the
// target faction is named so a SetInfiltration's ActionTarget filter can find it. The mission's
// own generator drives any roll, so a seeded caller stays reproducible.
TriggeredEffectContext_t MissionContext_(Faction& rActor, BaseManager& rBase,
                                         GameState& rGameState, std::mt19937& rRng)
{
    TriggeredEffectContext_t context(rGameState, rActor);
    context.pBase = &rBase;
    context.pTile = &rBase.GetTile();
    context.actionTarget = rBase.GetFactionId();
    context.pRng = &rRng;
    return context;
}

bool ApplyInfiltrate_(Faction& rActor, BaseManager& rBase, GameState& rGameState,
                      const ProbeActionConfig_t& rAction, ProbeActionResult_t& rResult,
                      std::mt19937& rRng)
{
    TriggeredEffectContext_t context = MissionContext_(rActor, rBase, rGameState, rRng);
    ApplyTriggeredEffects(rAction.onSuccessEffects, context);
    rResult.detail = ProbeActionStatus_t::Infiltrated;
    return true;
}

// Among techs the actor can research (prereqs met, not yet known) that the target already
// knows, pick uniformly at random. Empty when nothing is eligible.
TechId PickStealableTech_(const Faction& rActor, const Faction& rTarget, std::mt19937& rRng)
{
    std::vector<TechId> candidates;
    for (const TechConfig_t* pTech : rActor.GetResearch().GetAvailableTechs())
    {
        if (pTech && rTarget.GetResearch().HasDiscoveredTech(pTech->id))
        {
            candidates.push_back(pTech->id);
        }
    }
    if (candidates.empty())
    {
        return {};
    }
    std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
    return candidates[dist(rRng)];
}

bool ApplyStealTech_(Faction& rActor, BaseManager& rBase, ProbeActionResult_t& rResult,
                     std::mt19937& rRng)
{
    Faction& rTarget = rBase.GetFaction();
    const TechId techId = PickStealableTech_(rActor, rTarget, rRng);
    if (techId.empty())
    {
        rResult.detail = ProbeActionStatus_t::NoTech;
        return true; // mission succeeds but nothing to steal
    }
    rActor.GetResearch().AddDiscoveredTech(techId);
    rResult.detail = ProbeStolenTech_t{techId};
    return true;
}

int StealEnergyAmount_(const BaseManager& rBase, const Faction& rTarget, int morale)
{
    const int energy = rTarget.GetEconomy().GetEnergy();
    if (energy <= 0)
    {
        return 0;
    }
    const int pop = rBase.GetPopulation().GetSize();
    const int factionPop = rTarget.TotalPopulation();
    const int share = energy * pop / (factionPop + 1);
    const int minSteal = std::max(0, (share * std::max(1, morale)) / 10);
    return std::clamp(minSteal + std::max(0, (share - minSteal) / 2), 0, energy);
}

bool ApplyDrainEnergy_(Unit& rProbe, BaseManager& rBase, GameState& rGameState,
                       ProbeActionResult_t& rResult)
{
    Faction& rTarget = rBase.GetFaction();
    Faction& rActor = rProbe.GetFaction();
    EffectContext_t ctx;
    const int morale = rGameState.GetMoraleCalculator().EffectiveMoraleLevel(rProbe, ctx);
    const int stolen = StealEnergyAmount_(rBase, rTarget, morale);
    // StealEnergyAmount_ clamps to the target's treasury, so this can never overdraw.
    rTarget.GetEconomy().SpendEnergy(stolen);
    rActor.GetEconomy().AddEnergy(stolen);
    rResult.detail = ProbeEnergyStolen_t{stolen};
    return true;
}

bool ApplySabotage_(Faction& rActor, GameState& rGameState, BaseManager& rBase,
                    const ProbeActionConfig_t& rAction,
                    const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                    std::mt19937& rRng)
{
    // Targeted sabotage is a distinct action from random sabotage, so an empty or unknown
    // facility id is a failed action — not a silent fall-through to the random branch, and not
    // a ProbeDestroyedFacility_t report after DestroyBuilding's documented no-op.
    if (rAction.id == ProbeActionId_t::SabotageFacility)
    {
        const BuildingConfig_t* pTarget =
            facilityId.empty() ? nullptr : rBase.GetBuildingManager().FindBuilding(facilityId);
        if (!pTarget)
        {
            rResult.detail = ProbeActionStatus_t::NoTarget;
            return false;
        }
        DestroyBuildingAndNotify(rGameState, rBase, *pTarget);
        rResult.detail = ProbeDestroyedFacility_t{facilityId};
        return true;
    }

    // Random: the action's DestroyFacility entry carries the target policy (which facilities
    // are off-limits), and the dispatcher reports back what it actually destroyed. When
    // nothing was eligible, wipe current production instead.
    TriggeredEffectContext_t context = MissionContext_(rActor, rBase, rGameState, rRng);
    for (const TriggeredEffectResult_t& rResultEntry :
         ApplyTriggeredEffects(rAction.onSuccessEffects, context))
    {
        const auto* pDestroyed = std::get_if<FacilitiesDestroyed_t>(&rResultEntry);
        if (pDestroyed && !pDestroyed->buildingIds.empty())
        {
            rResult.detail = ProbeDestroyedFacility_t{pDestroyed->buildingIds.front()};
            return true;
        }
    }

    rBase.GetProduction().SetProduction(nullptr, rBase.GetBaseEffects());
    rBase.GetProduction().SetMineralStockpile(0);
    rResult.detail = ProbeActionStatus_t::ProductionWiped;
    return true;
}

bool ApplyInciteDroneRiots_(BaseManager& rBase, const ProbeActionConfig_t& rConfig,
                            ProbeActionResult_t& rResult)
{
    rBase.GetPopulation().ForceRiot(rConfig.riotTurns);
    rResult.detail = ProbeActionStatus_t::Riot;
    return true;
}

bool ApplyAssassinate_(BaseManager& rBase, ProbeActionResult_t& rResult)
{
    rBase.GetFaction().GetResearch().SetAccumulatedPoints(0);
    rResult.detail = ProbeActionStatus_t::ResearchWiped;
    return true;
}

bool ApplyMindControlBase_(Faction& rActor, BaseManager& rBase, ProbeActionResult_t& rResult)
{
    Faction& rTarget = rBase.GetFaction();
    const BaseId_t baseId = rBase.GetBaseId();
    rBase.GetPopulation().NotifyCaptured(rTarget.GetFactionId(), rActor.GetFactionId());
    rTarget.TransferBaseTo(baseId, rActor);
    rResult.detail = ProbeActionStatus_t::BaseCaptured;
    return true;
}

bool ApplyGeneticPlague_(Faction& rActor, BaseManager& rBase, GameState& rGameState,
                         const ProbeActionConfig_t& rAction, ProbeActionResult_t& rResult,
                         std::mt19937& rRng)
{
    TriggeredEffectContext_t context = MissionContext_(rActor, rBase, rGameState, rRng);
    int killed = 0;
    for (const TriggeredEffectResult_t& rResultEntry :
         ApplyTriggeredEffects(rAction.onSuccessEffects, context))
    {
        if (const auto* pChanged = std::get_if<PopulationChanged_t>(&rResultEntry))
        {
            killed += std::max(0, -pChanged->delta);
        }
    }
    rResult.detail = ProbePopulationKilled_t{killed};
    return true;
}

bool ApplySubvertUnit_(Faction& rActor, Unit& rTargetUnit, ProbeActionResult_t& rResult)
{
    rTargetUnit.GetFaction().TransferUnitTo(rTargetUnit.GetUnitId(), rActor);
    rResult.detail = ProbeActionStatus_t::UnitSubverted;
    return true;
}

bool ApplyBaseAction_(Unit& rProbe, const ProbeActionConfig_t& rAction, BaseManager& rBase,
                      GameState& rGameState, const GameDataContext& rDataContext,
                      const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                      std::mt19937& rRng)
{
    Faction& rActor = rProbe.GetFaction();

    switch (rAction.id)
    {
        case ProbeActionId_t::Infiltrate:
            return ApplyInfiltrate_(rActor, rBase, rGameState, rAction, rResult, rRng);
        case ProbeActionId_t::StealTech:
            return ApplyStealTech_(rActor, rBase, rResult, rRng);
        case ProbeActionId_t::DrainEnergy:
            return ApplyDrainEnergy_(rProbe, rBase, rGameState, rResult);
        case ProbeActionId_t::SabotageRandom:
        case ProbeActionId_t::SabotageFacility:
            return ApplySabotage_(rActor, rGameState, rBase, rAction, facilityId, rResult, rRng);
        case ProbeActionId_t::InciteDroneRiots:
            return ApplyInciteDroneRiots_(rBase, rAction, rResult);
        case ProbeActionId_t::Assassinate:
            return ApplyAssassinate_(rBase, rResult);
        case ProbeActionId_t::MindControlBase:
        case ProbeActionId_t::TotalThoughtControl:
            return ApplyMindControlBase_(rActor, rBase, rResult);
        case ProbeActionId_t::GeneticPlague:
            return ApplyGeneticPlague_(rActor, rBase, rGameState, rAction, rResult, rRng);
        case ProbeActionId_t::SubvertUnit:
            break;
    }
    return false;
}

bool ApplyUnitAction_(Unit& rProbe, const ProbeActionConfig_t& rAction, Unit& rTargetUnit,
                      ProbeActionResult_t& rResult)
{
    if (rAction.id == ProbeActionId_t::SubvertUnit)
    {
        return ApplySubvertUnit_(rProbe.GetFaction(), rTargetUnit, rResult);
    }
    return false;
}

} // namespace

bool ApplyProbeActionEffect(Unit& rProbe, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget, GameState& rGameState,
                            const GameDataContext& rDataContext,
                            const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                            std::mt19937& rRng)
{
    return std::visit(
        [&](const auto& rConcrete) -> bool
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, ProbeBaseTarget_t>)
            {
                return ApplyBaseAction_(rProbe, rAction, rConcrete.rBase, rGameState,
                                        rDataContext, facilityId, rResult, rRng);
            }
            else
            {
                return ApplyUnitAction_(rProbe, rAction, rConcrete.rUnit, rResult);
            }
        },
        rTarget.ref);
}

} // namespace ac
