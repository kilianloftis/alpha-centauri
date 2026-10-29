#include "game/probes/ProbeActionMutations.h"

#include "game/probes/ProbeRules.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/economy/EconomyManager.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingMutations.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/production/IConstructable.h"
#include "game/production/ProductionManager.h"
#include "game/map/WorldMap.h"
#include "game/map/MapUtils.h"
#include "game/map/UnitPositionIndex.h"
#include "game/research/TechConfigParser.h"
#include "game/combat/MoraleCalculator.h"
#include "game/combat/MoraleRules.h"
#include "game/units/Unit.h"
#include "game/visibility/VisibilityMutations.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"
#include "lib/RandomRoll.h"

#include "game/effects/EffectEnums.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace ac
{

namespace
{

// The actor is the subject, the target's base is acted on, and its faction is named for
// ActionTarget filters. The mission's own RNG keeps seeded callers reproducible.
TriggeredEffectContext_t MissionContext_(Faction& rActor, BaseManager& rBase,
                                         GameState& rGameState, std::mt19937& rRng)
{
    TriggeredEffectContext_t context(rGameState, rActor);
    context.pBase = &rBase;
    context.pTile = &rBase.GetTile();
    context.pFaction = &rBase.GetFaction();
    context.actionTarget = rBase.GetFactionId();
    context.pRng = &rRng;
    return context;
}

// The unit-target counterpart of MissionContext_: the targeted unit is the unit subject and its
// owner is the action target.
TriggeredEffectContext_t UnitMissionContext_(Faction& rActor, Unit& rTargetUnit,
                                             GameState& rGameState, std::mt19937& rRng)
{
    const Tile& rTile = rTargetUnit.GetTile();
    Tile* const pTile = rGameState.GetWorldMap().GetTile(rTile.GetX(), rTile.GetY());
    if (!pTile)
    {
        throw std::runtime_error("UnitMissionContext_: target unit tile is not on the world map");
    }
    TriggeredEffectContext_t context(rGameState, rActor);
    context.pUnit = &rTargetUnit;
    context.pTile = pTile;
    context.pFaction = &rTargetUnit.GetFaction();
    context.actionTarget = rTargetUnit.GetFaction().GetFactionId();
    context.pRng = &rRng;
    return context;
}

bool ApplyInfiltrate_(Faction& rActor, BaseManager& rBase, GameState& rGameState,
                      const ProbeActionConfig_t& rAction, ProbeActionResult_t& rResult,
                      std::mt19937& rRng)
{
    TriggeredEffectContext_t context = MissionContext_(rActor, rBase, rGameState, rRng);
    ApplyTriggeredEffects(rAction.onSuccessEffects, context);
    rResult.detail = ProbeInfiltrated_t{};
    return true;
}

bool StealMapOrNothing_(Faction& rActor, const Faction& rTarget, ProbeActionResult_t& rResult)
{
    if (CopyExploredMap(rActor, rTarget))
    {
        rResult.detail = ProbeStolenMap_t{};
        return true;
    }
    rResult.detail = ProbeNoTech_t{};
    return true;
}

bool StealRandomTech_(Faction& rActor, const std::vector<TechId>& rCandidates,
                      ProbeActionResult_t& rResult, std::mt19937& rRng)
{
    const int bound = static_cast<int>(rCandidates.size());
    const int index = RollExclusiveMax(bound, rRng);
    const size_t chosen = static_cast<size_t>(index);
    const TechId techId = rCandidates[chosen];
    rActor.GetResearch().AddDiscoveredTech(techId);
    rResult.detail = ProbeStolenTech_t{techId};
    return true;
}

bool ApplyStealTech_(Faction& rActor, BaseManager& rBase, ProbeActionResult_t& rResult,
                     std::mt19937& rRng)
{
    rBase.SetStolenResearch();
    const std::vector<TechId> candidates = StealableTechs(rActor, rBase.GetFaction());
    if (candidates.empty())
    {
        return StealMapOrNothing_(rActor, rBase.GetFaction(), rResult);
    }
    return StealRandomTech_(rActor, candidates, rResult, rRng);
}

bool ApplyDrainEnergy_(Unit& rProbe, BaseManager& rBase, GameState& rGameState,
                       const ProbeActionConfig_t& rAction, ProbeActionResult_t& rResult,
                       std::mt19937& rRng)
{
    Faction& rTarget = rBase.GetFaction();
    Faction& rActor = rProbe.GetFaction();
    EffectContext_t ctx;
    const MoraleCalculator& rMorale = *rGameState.GetGameData().moraleCalculator;
    const int morale = EffectiveMoraleLevel(rProbe, ctx, rMorale);
    const StealEnergyQuote_t quote = QuoteStealEnergy(rAction, rBase, rTarget, morale);
    const int remainderDraw = RollExclusiveMax(quote.remainderBound, rRng);
    const int stolen = StealEnergyAmount(rAction, quote, remainderDraw);
    rTarget.GetEconomy().SpendEnergy(stolen);
    rActor.GetEconomy().AddEnergy(stolen);
    rResult.detail = ProbeEnergyStolen_t{stolen};
    return true;
}

bool WipeProduction_(BaseManager& rBase, ProbeActionResult_t& rResult)
{
    const IConstructable* pItem = rBase.GetProduction().GetCurrentProduction();
    const std::string itemName = pItem ? pItem->GetName() : std::string{};
    rBase.GetProduction().SetProduction(nullptr, rBase.GetBaseEffects());
    rBase.GetProduction().SetMineralStockpile(0);
    rResult.detail = ProbeProductionWiped_t{itemName};
    return true;
}

// Targeted sabotage with an unknown facility id fails; an empty id wipes production.
bool ApplyTargetedSabotage_(BaseManager& rBase, const BuildingId_t& facilityId,
                            ProbeActionResult_t& rResult)
{
    if (facilityId.empty())
    {
        return WipeProduction_(rBase, rResult);
    }
    const BuildingConfig_t* pTarget = rBase.GetBuildingManager().FindBuilding(facilityId);
    if (!pTarget)
    {
        rResult.detail = ProbeNoTarget_t{};
        return false;
    }
    rBase.GetBuildingManager().DestroyBuilding(pTarget->id);
    rResult.detail = ProbeDestroyedFacility_t{facilityId};
    return true;
}

bool ApplySabotage_(Faction& rActor, GameState& rGameState, BaseManager& rBase,
                    const ProbeActionConfig_t& rAction,
                    const BuildingId_t& facilityId, ProbeActionResult_t& rResult,
                    std::mt19937& rRng)
{
    if (rAction.id == ProbeActionId_t::SabotageFacility)
    {
        return ApplyTargetedSabotage_(rBase, facilityId, rResult);
    }
    // The DestroyFacility entry carries which facilities are off-limits. With nothing eligible,
    // current production is wiped instead.
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

    return WipeProduction_(rBase, rResult);
}

bool ApplyInciteDroneRiots_(BaseManager& rBase, const ProbeActionConfig_t& rConfig,
                            ProbeActionResult_t& rResult)
{
    PopulationManager& rPopulation = rBase.GetPopulation();
    rPopulation.ApplyIncite(rConfig.assimilationTurnsAdded, rConfig.assimilationTurnsCapFactor);
    if (rPopulation.IsRioting())
    {
        rResult.detail = ProbeRiot_t{};
    }
    else
    {
        rResult.detail = ProbeIncreasedActivity_t{};
    }
    return true;
}

bool ApplyAssassinate_(BaseManager& rBase, ProbeActionResult_t& rResult, std::mt19937& rRng)
{
    ResearchManager& rResearch = rBase.GetFaction().GetResearch();
    const int stock = rResearch.GetAccumulatedPoints();
    const int lost = RollExclusiveMax(stock, rRng);
    rResearch.SetAccumulatedPoints(stock - lost);
    rResult.detail = ProbeResearchWiped_t{lost};
    return true;
}

void TransferCapturedGarrison_(BaseManager& rCaptured, Faction& rOldOwner, Faction& rActor,
                               GameState& rGameState)
{
    WorldMap& rMap = rGameState.GetWorldMap();
    const FactionId_t oldOwnerId = rOldOwner.GetFactionId();
    std::vector<UnitId_t> unitIds;
    ForEachTileInChebyshevRadius(rCaptured.GetTile(), rMap, 1,
        [&](Tile* pTile, int distance)
        {
            if (!pTile)
            {
                return;
            }
            if (distance == 1 && rGameState.FindBaseAt(pTile->GetX(), pTile->GetY()) != nullptr)
            {
                return;
            }
            for (Unit* pUnit : rMap.GetUnitPositions().GetUnitsOnTile(*pTile))
            {
                if (pUnit && pUnit->GetFaction().GetFactionId() == oldOwnerId)
                {
                    unitIds.push_back(pUnit->GetUnitId());
                }
            }
        });
    for (const UnitId_t unitId : unitIds)
    {
        rOldOwner.TransferUnitTo(unitId, rActor);
    }
}

bool ApplyMindControlBase_(Faction& rActor, BaseManager& rBase, GameState& rGameState,
                           ProbeActionResult_t& rResult)
{
    Faction& rTarget = rBase.GetFaction();
    const BaseId_t baseId = rBase.GetBaseId();
    rBase.GetPopulation().NotifyCaptured(rTarget.GetFactionId(), rActor.GetFactionId());
    rTarget.TransferBaseTo(baseId, rActor);
    TransferCapturedGarrison_(rBase, rTarget, rActor, rGameState);
    rResult.detail = ProbeBaseCaptured_t{};
    return true;
}

void DamagePlagueGarrison_(const ProbeActionConfig_t& rAction, BaseManager& rBase,
                           GameState& rGameState, std::mt19937& rRng)
{
    for (Unit* pUnit : rGameState.GetWorldMap().GetAllUnitsOnTile(rBase.GetTile()))
    {
        const int hp = pUnit->GetCurrentHp();
        const int draw = RollExclusiveMax(PlagueGarrisonDrawBound(rAction, hp), rRng);
        const int damage = PlagueGarrisonDamage(rAction, rBase, hp, draw);
        pUnit->SetCurrentHp(hp - damage);
    }
}

void ApplyPlagueKills_(BaseManager& rBase, int killed)
{
    for (int i = 0; i < killed; ++i)
    {
        rBase.GetPopulation().RemovePop();
    }
}

bool ApplyGeneticPlague_(Faction& rActor, BaseManager& rBase, GameState& rGameState,
                         const ProbeActionConfig_t& rAction, ProbeActionResult_t& rResult,
                         std::mt19937& rRng)
{
    const int killed = GeneticPlagueKilled(rAction, rBase);
    DamagePlagueGarrison_(rAction, rBase, rGameState, rRng);
    ApplyPlagueKills_(rBase, killed);
    rBase.GetPopulation().SetNutrientStockpile(0);
    rBase.SetPlagueIntroduced();
    TriggeredEffectContext_t context = MissionContext_(rActor, rBase, rGameState, rRng);
    ApplyTriggeredEffects(rAction.onSuccessEffects, context);
    rResult.detail = ProbePopulationKilled_t{killed};
    return true;
}

bool ApplySubvertUnit_(Faction& rActor, Unit& rTargetUnit, GameState& rGameState,
                       const ProbeActionConfig_t& rAction, ProbeActionResult_t& rResult,
                       std::mt19937& rRng)
{
    TriggeredEffectContext_t context = UnitMissionContext_(rActor, rTargetUnit, rGameState, rRng);
    ApplyTriggeredEffects(rAction.onSuccessEffects, context);
    rTargetUnit.GetFaction().TransferUnitTo(rTargetUnit.GetUnitId(), rActor);
    rResult.detail = ProbeUnitSubverted_t{};
    return true;
}

bool ApplyBaseAction_(Unit& rProbe, const ProbeActionConfig_t& rAction, BaseManager& rBase,
                      GameState& rGameState, const BuildingId_t& facilityId,
                      ProbeActionResult_t& rResult, std::mt19937& rRng)
{
    Faction& rActor = rProbe.GetFaction();

    switch (rAction.id)
    {
        case ProbeActionId_t::Infiltrate:
            return ApplyInfiltrate_(rActor, rBase, rGameState, rAction, rResult, rRng);
        case ProbeActionId_t::StealTech:
            return ApplyStealTech_(rActor, rBase, rResult, rRng);
        case ProbeActionId_t::DrainEnergy:
            return ApplyDrainEnergy_(rProbe, rBase, rGameState, rAction, rResult, rRng);
        case ProbeActionId_t::SabotageRandom:
        case ProbeActionId_t::SabotageFacility:
            return ApplySabotage_(rActor, rGameState, rBase, rAction, facilityId, rResult, rRng);
        case ProbeActionId_t::InciteDroneRiots:
            return ApplyInciteDroneRiots_(rBase, rAction, rResult);
        case ProbeActionId_t::Assassinate:
            return ApplyAssassinate_(rBase, rResult, rRng);
        case ProbeActionId_t::MindControlBase:
        case ProbeActionId_t::TotalThoughtControl:
            return ApplyMindControlBase_(rActor, rBase, rGameState, rResult);
        case ProbeActionId_t::GeneticPlague:
            return ApplyGeneticPlague_(rActor, rBase, rGameState, rAction, rResult, rRng);
        case ProbeActionId_t::SubvertUnit:
            break;
    }
    return false;
}

bool ApplyUnitAction_(Unit& rProbe, const ProbeActionConfig_t& rAction, Unit& rTargetUnit,
                      GameState& rGameState, ProbeActionResult_t& rResult, std::mt19937& rRng)
{
    if (rAction.id == ProbeActionId_t::SubvertUnit)
    {
        return ApplySubvertUnit_(rProbe.GetFaction(), rTargetUnit, rGameState, rAction, rResult,
                                 rRng);
    }
    return false;
}

} // namespace

bool ApplyProbeActionEffect(Unit& rProbe, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget, GameState& rGameState,
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
                                        facilityId, rResult, rRng);
            }
            else
            {
                return ApplyUnitAction_(rProbe, rAction, rConcrete.rUnit, rGameState, rResult,
                                        rRng);
            }
        },
        rTarget.ref);
}

void ApplyProbePaidEffects(Unit& rProbe, const ProbeActionConfig_t& rAction,
                           const ProbeTarget_t& rTarget, GameState& rGameState,
                           std::mt19937& rRng)
{
    Faction& rActor = rProbe.GetFaction();
    TriggeredEffectContext_t context = std::visit(
        [&](const auto& rConcrete) -> TriggeredEffectContext_t
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, ProbeBaseTarget_t>)
            {
                return MissionContext_(rActor, rConcrete.rBase, rGameState, rRng);
            }
            else
            {
                return UnitMissionContext_(rActor, rConcrete.rUnit, rGameState, rRng);
            }
        },
        rTarget.ref);
    ApplyTriggeredEffects(rAction.onPaidEffects, context);
}

namespace
{

// A failed draw may still be saved and a successful one busted. RollPercent leaves the RNG alone
// at 0 and 100, so a probe with neither chance draws only what the binary draws.
bool ApplySaveAndBust_(bool bDrawSucceeded, const ProbeChance_t& rChances, std::mt19937& rRng)
{
    if (!bDrawSucceeded)
    {
        return RollPercent(rChances.failureSaveChance, rRng);
    }
    return !RollPercent(rChances.successBustChance, rRng);
}

void EndPact_(GameState& rGameState, FactionId_t a, FactionId_t b)
{
    const DiplomaticStatus_t status = rGameState.GetDiplomacyLedger().GetStatus(a, b);
    ApplyStatusChange(rGameState, a, b, StepDown(status).value());
}

void ApplyBreakOutcome_(GameState& rGameState, const HostileAct_t& rAct, BreakOutcome_t outcome)
{
    switch (outcome)
    {
        case BreakOutcome_t::HostileAct:
            ApplyHostileAct(rGameState, rAct);
            return;
        case BreakOutcome_t::PactEnded:
            EndPact_(rGameState, rAct.aggressor, rAct.victim);
            return;
        case BreakOutcome_t::Vendetta:
            ApplyProbeVendetta(rGameState, rAct.aggressor, rAct.victim);
            return;
    }
    throw std::logic_error("ApplyBreakOutcome_: unknown break outcome");
}

} // namespace

void ApplyProbeAftermath(GameState& rGameState, const HostileAct_t& rAct,
                         const ProbeAftermath_t& rAftermath, std::mt19937& rRng)
{
    // TODO: AI attitude model. Until it exists every victim that may keep does. A player
    // victim should get SMAC's EXCUSE / PACTEXCUSE choice; no AI probes the player yet.
    if (rAftermath.bMayKeepAgreement)
    {
        const int friction = RollExclusiveMax(rAftermath.keepFrictionDraw, rRng);
        ApplyFriction(rGameState, rAct.victim, rAct.aggressor, friction);
        return;
    }
    ApplyBreakOutcome_(rGameState, rAct, rAftermath.breakOutcome);
}

ProbeRollResult_t RollProbeAction(const ProbeChance_t& rChances, std::mt19937& rRng)
{
    ProbeRollResult_t result;
    result.chances = rChances;

    const int missionDraw = RollExclusiveMax(rChances.probeStrength, rRng);
    const bool bMissionDraw = MissionDrawSucceeds(rChances, missionDraw);
    result.missionSucceeded = ApplySaveAndBust_(bMissionDraw, rChances, rRng);
    if (!result.missionSucceeded)
    {
        return result;
    }

    const int escapeDraw = RollExclusiveMax(rChances.escapeStrength, rRng);
    const bool bEscapeDraw = EscapeDrawSucceeds(rChances, escapeDraw);
    result.escaped = ApplySaveAndBust_(bEscapeDraw, rChances, rRng);
    return result;
}

bool TryPromoteProbeMission(Unit& rProbe, const MoraleCalculator& rMorale)
{
    if (!ResolveFlag(rProbe, RuleFlagId_t::ProbeTeam))
    {
        return false;
    }
    const int xp = rProbe.GetXp();
    if (xp >= rMorale.GetConfig().MaxLevel())
    {
        return false;
    }
    rProbe.SetXp(xp + 1);
    return true;
}

} // namespace ac
