#include "game/faction/DiplomaticTransitionEffects.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyConfig.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomaticPermissionRules.h"
#include "game/faction/DiplomaticTransitionRules.h"
#include "game/PlayerInteractionQueue.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/EvacuateTerritoryEffects.h"

#include <optional>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

bool HasDiplomacy_(const Faction& rFaction)
{
    return !IsNativeLifeFaction(rFaction.GetDefinition().identity.species);
}

// TODO: read from config/diplomacy.json.
constexpr DefensiveObligationMode_t k_DefensiveObligationMode =
    DefensiveObligationMode_t::JoinAsDefender;

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

void WithdrawOnDeclaration_(GameState& rGameState, FactionId_t a, FactionId_t b,
                            VendettaEntry_t entry)
{
    if (entry != VendettaEntry_t::Declaration)
    {
        return;
    }
    Faction* pA = rGameState.FindFaction(a);
    Faction* pB = rGameState.FindFaction(b);
    if (!pA || !pB || !HasDiplomacy_(*pA) || !HasDiplomacy_(*pB))
    {
        return;
    }
    WithdrawFromTerritories_(*pA, *pB, rGameState.GetWorldMap(),
                             rGameState.GetGameData().interactionGrids);
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

void DeclareVendetta_(GameState& rGameState, FactionId_t declarer, FactionId_t target,
                    VendettaEntry_t entry)
{
    if (declarer == target)
    {
        throw std::invalid_argument("DeclareVendetta_: a faction cannot declare on itself");
    }
    const DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    if (rLedger.HasVendetta(declarer, target))
    {
        return;
    }
    ApplyStatusChange(rGameState, declarer, target, DiplomaticStatus_t::Vendetta);
    WithdrawOnDeclaration_(rGameState, declarer, target, entry);

    const Faction* pDeclarer = rGameState.FindFaction(declarer);
    const Faction* pTarget = rGameState.FindFaction(target);
    if (!pDeclarer || !pTarget || !HasDiplomacy_(*pDeclarer) || !HasDiplomacy_(*pTarget))
    {
        return;
    }

    std::vector<FactionId_t> partners;
    for (const Faction& rPartner : rGameState.Factions())
    {
        const FactionId_t partnerId = rPartner.GetFactionId();
        if (partnerId == declarer || partnerId == target || !HasDiplomacy_(rPartner)
            || !StatusRulesFor(rGameState, partnerId, target).bDefensiveObligation
            || rLedger.HasVendetta(partnerId, declarer))
        {
            continue;
        }
        partners.push_back(partnerId);
    }

    for (const FactionId_t partnerId : partners)
    {
        if (rGameState.FindFaction(partnerId)->IsPlayerControlled())
        {
            if (!IsObligationQueued_(rGameState, target, declarer))
            {
                EnqueueForPlayer(rGameState,
                                 PactObligationInteraction_t{target, declarer, entry});
            }
            continue;
        }
        ResolveDefensiveObligation(
            rGameState, partnerId, target, declarer,
            AiHonorsDefensiveObligation_(rGameState, partnerId, target, declarer), entry);
    }
}

void JoinVendetta_(GameState& rGameState, FactionId_t defender, FactionId_t aggressor,
                    VendettaEntry_t entry)
{
    if (defender == aggressor)
    {
        throw std::invalid_argument("JoinVendetta_: a faction cannot defend against itself");
    }
    if (!rGameState.GetDiplomacyLedger().HasVendetta(defender, aggressor))
    {
        ApplyStatusChange(rGameState, defender, aggressor, DiplomaticStatus_t::Vendetta);
        WithdrawOnDeclaration_(rGameState, defender, aggressor, entry);
    }
}

} // namespace

void ApplyStatusChange(GameState& rGameState, FactionId_t a, FactionId_t b,
                       DiplomaticStatus_t to)
{
    if (a == b)
    {
        throw std::invalid_argument("ApplyStatusChange: a faction has no status with itself");
    }

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

    Faction* pA = rGameState.FindFaction(a);
    Faction* pB = rGameState.FindFaction(b);
    if (!pA || !pB)
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
        WithdrawFromTerritories_(*pA, *pB, rWorldMap, rGrids);
    }
    if (rFrom.bShareTiles && !rTo.bShareTiles)
    {
        EvacuateUnitsSharingWith(*pA, *pB, rWorldMap, rGrids);
        EvacuateUnitsSharingWith(*pB, *pA, rWorldMap, rGrids);
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
            ApplyStatusChange(rGameState, rPair.a, rPair.b, *StepDown(status));
        }
    }
}

void DeclareVendetta(GameState& rGameState, FactionId_t declarer, FactionId_t target)
{
    DeclareVendetta_(rGameState, declarer, target, VendettaEntry_t::Declaration);
}

void JoinVendetta(GameState& rGameState, FactionId_t defender, FactionId_t aggressor)
{
    JoinVendetta_(rGameState, defender, aggressor, VendettaEntry_t::Declaration);
}

void HonorDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t aggressor,
                              DefensiveObligationMode_t mode, VendettaEntry_t entry)
{
    switch (mode)
    {
    case DefensiveObligationMode_t::JoinAsDefender:
        JoinVendetta_(rGameState, partner, aggressor, entry);
        return;
    case DefensiveObligationMode_t::SeparateDeclaration:
        DeclareVendetta_(rGameState, partner, aggressor, entry);
        return;
    }
}

void ApplyHostileAct(GameState& rGameState, FactionId_t aggressor, FactionId_t victim)
{
    if (aggressor == victim)
    {
        throw std::invalid_argument("ApplyHostileAct: a faction cannot attack itself");
    }
    const Faction* pAggressor = rGameState.FindFaction(aggressor);
    const Faction* pVictim = rGameState.FindFaction(victim);
    if (!pAggressor || !pVictim || !HasDiplomacy_(*pAggressor) || !HasDiplomacy_(*pVictim))
    {
        return;
    }
    if (!StatusRulesFor(rGameState, aggressor, victim).bMayAttack)
    {
        DeclareVendetta_(rGameState, aggressor, victim, VendettaEntry_t::SneakAttack);
    }
}

void ResolveDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t ally,
                                FactionId_t aggressor, bool bDeclareVendetta,
                                VendettaEntry_t entry)
{
    if (bDeclareVendetta)
    {
        HonorDefensiveObligation(rGameState, partner, aggressor, k_DefensiveObligationMode,
                                 entry);
        return;
    }
    const DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomaticStatus_t status = rLedger.GetStatus(partner, ally);
    if (rGameState.GetGameData().diplomacyConfig->For(status).bDefensiveObligation)
    {
        if (const std::optional<DiplomaticStatus_t> lower = StepDown(status))
        {
            ApplyStatusChange(rGameState, partner, ally, *lower);
        }
    }
}

} // namespace ac
