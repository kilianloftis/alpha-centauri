#include "game/faction/DiplomaticTransitionEffects.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyConfig.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticPermissionRules.h"
#include "game/faction/DiplomaticTransitionRules.h"
#include "game/PlayerInteractionQueue.h"
#include "game/units/EvacuateTerritoryEffects.h"

#include <optional>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

// TODO: AI attitude model. Until it exists every AI honors its obligation.
bool AiHonorsDefensiveObligation_(const GameState& /*rGameState*/, FactionId_t /*partner*/,
                                  FactionId_t /*ally*/, FactionId_t /*aggressor*/)
{
    return true;
}

void WithdrawFromTerritories_(Faction& rA, Faction& rB, WorldMap& rWorldMap,
                              const InteractionGridsConfig_t& rGrids)
{
    EvacuateUnitsFromTerritory(rA, rB.GetFactionId(), rWorldMap, rGrids);
    EvacuateUnitsFromTerritory(rB, rA.GetFactionId(), rWorldMap, rGrids);
}

bool IsObligationQueued_(const GameState& rGameState, FactionId_t ally, FactionId_t aggressor)
{
    return rGameState.GetPlayerInteractions().AnyOf(
        [&](const QueuedInteraction_t& rQueued)
        {
            const auto* pObligation =
                std::get_if<PactObligationInteraction_t>(&rQueued.payload);
            return pObligation && pObligation->allyId == ally
                && pObligation->aggressorId == aggressor;
        });
}

// False when the pair is already at Vendetta or either side has no diplomacy.
bool EnterVendetta_(GameState& rGameState, FactionId_t a, FactionId_t b, VendettaKind_t kind)
{
    if (rGameState.GetDiplomacyLedger().HasVendetta(a, b))
    {
        return false;
    }
    Faction& rA = rGameState.RequireFaction(a);
    Faction& rB = rGameState.RequireFaction(b);
    if (!HasDiplomacy(rA) || !HasDiplomacy(rB))
    {
        return false;
    }
    ApplyStatusChange(rGameState, a, b, DiplomaticStatus_t::Vendetta);
    if (kind == VendettaKind_t::Declaration)
    {
        WithdrawFromTerritories_(rA, rB, rGameState.GetWorldMap(),
                                 rGameState.GetGameData().interactionGrids);
    }
    return true;
}

// Each faction is checked as the loop reaches it, so a later partner sees an earlier one's
// answer.
void RaiseDefensiveObligations_(GameState& rGameState, FactionId_t aggressor, FactionId_t ally,
                                VendettaKind_t kind)
{
    for (const Faction& rPartner : rGameState.Factions())
    {
        const FactionId_t partner = rPartner.GetFactionId();
        if (!IsObligedToDefend(rGameState, partner, ally, aggressor))
        {
            continue;
        }
        if (rPartner.IsPlayerControlled())
        {
            if (!IsObligationQueued_(rGameState, ally, aggressor))
            {
                EnqueueForPlayer(rGameState, PactObligationInteraction_t{ally, aggressor, kind});
            }
        }
        else if (AiHonorsDefensiveObligation_(rGameState, partner, ally, aggressor))
        {
            HonorDefensiveObligation(rGameState, partner, aggressor, kind);
        }
        else
        {
            DeclineDefensiveObligation(rGameState, partner, ally);
        }
    }
}

void DeclareVendetta_(GameState& rGameState, FactionId_t declarer, FactionId_t target,
                      VendettaKind_t kind)
{
    if (EnterVendetta_(rGameState, declarer, target, kind))
    {
        RaiseDefensiveObligations_(rGameState, declarer, target, kind);
    }
}

} // namespace

void ApplyStatusChange(GameState& rGameState, FactionId_t a, FactionId_t b,
                       DiplomaticStatus_t to)
{
    Faction& rA = rGameState.RequireFaction(a);
    Faction& rB = rGameState.RequireFaction(b);
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomaticStatus_t from = rLedger.GetStatus(a, b);
    rLedger.SetStatus(a, b, to);
    if (to == DiplomaticStatus_t::Vendetta)
    {
        rLedger.SetKnown(a, b);
    }
    if (from == to)
    {
        return;
    }

    const GameDataContext& rData = rGameState.GetGameData();
    const DiplomaticStatusRules_t& rFrom = rData.diplomacyConfig->For(from);
    const DiplomaticStatusRules_t& rTo = rData.diplomacyConfig->For(to);
    const InteractionGridsConfig_t& rGrids = rData.interactionGrids;
    WorldMap& rWorldMap = rGameState.GetWorldMap();

    if (rFrom.bEnterTerritory && !rTo.bEnterTerritory)
    {
        WithdrawFromTerritories_(rA, rB, rWorldMap, rGrids);
    }
    if (rFrom.bShareTiles && !rTo.bShareTiles)
    {
        EvacuateUnitsSharingWith(rA, rB, rWorldMap, rGrids);
        EvacuateUnitsSharingWith(rB, rA, rWorldMap, rGrids);
    }
}

void ExpireDiplomaticStatuses(GameState& rGameState)
{
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomacyConfig_t& rConfig = *rGameState.GetGameData().diplomacyConfig;
    rLedger.AgeStatuses();
    for (const FactionPair& rPair : rLedger.GetStatusPairs())
    {
        const DiplomaticStatus_t status = rLedger.GetStatus(rPair.a, rPair.b);
        const std::optional<int> duration = rConfig.For(status).durationTurns;
        if (duration && rLedger.GetTurnsHeld(rPair.a, rPair.b) >= *duration)
        {
            ApplyStatusChange(rGameState, rPair.a, rPair.b, StepDown(status).value());
        }
    }
}

void CancelTreaty(GameState& rGameState, FactionId_t a, FactionId_t b)
{
    const std::optional<DiplomaticStatus_t> lower =
        StepDown(rGameState.GetDiplomacyLedger().GetStatus(a, b));
    if (!lower)
    {
        throw std::logic_error("CancelTreaty: the pair holds no agreement to cancel");
    }
    ApplyStatusChange(rGameState, a, b, *lower);
}

void DeclareVendetta(GameState& rGameState, FactionId_t declarer, FactionId_t target)
{
    DeclareVendetta_(rGameState, declarer, target, VendettaKind_t::Declaration);
}

void JoinVendetta(GameState& rGameState, FactionId_t defender, FactionId_t aggressor)
{
    EnterVendetta_(rGameState, defender, aggressor, VendettaKind_t::Declaration);
}

void ApplyHostileAct(GameState& rGameState, FactionId_t aggressor, FactionId_t victim)
{
    if (!StatusRulesFor(rGameState, aggressor, victim).bMayAttack)
    {
        DeclareVendetta_(rGameState, aggressor, victim, VendettaKind_t::SneakAttack);
    }
}

void HonorDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t aggressor,
                              VendettaKind_t kind)
{
    switch (rGameState.GetGameData().diplomacyConfig->defensiveObligationMode)
    {
    case DefensiveObligationMode_t::JoinAsDefender:
        EnterVendetta_(rGameState, partner, aggressor, kind);
        return;
    case DefensiveObligationMode_t::SeparateDeclaration:
        DeclareVendetta_(rGameState, partner, aggressor, kind);
        return;
    }
}

void DeclineDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t ally)
{
    if (!StatusRulesFor(rGameState, partner, ally).bDefensiveObligation)
    {
        throw std::logic_error("DeclineDefensiveObligation: no defensive obligation to decline");
    }
    const DiplomaticStatus_t status = rGameState.GetDiplomacyLedger().GetStatus(partner, ally);
    ApplyStatusChange(rGameState, partner, ally, StepDown(status).value());
}

} // namespace ac
