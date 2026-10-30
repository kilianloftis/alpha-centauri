#include "game/combat/CaptureMutations.h"

#include "game/GameState.h"
#include "game/PlayerInteraction.h"
#include "game/PlayerInteractionQueue.h"
#include "game/combat/CombatResolver.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/economy/EconomyManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/UnitPositionIndex.h"
#include "game/movement/MovementConstants.h"
#include "game/natives/NativeRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitTransport.h"

#include <stdexcept>

namespace ac
{

namespace
{

void ChangeOwner_(Unit& rUnit, Faction& rCaptor)
{
    // Cargo changes hands with its carrier.
    if (&rUnit.GetFaction() != &rCaptor)
    {
        rUnit.GetFaction().TransferUnitTo(rUnit.GetUnitId(), rCaptor);
    }
}

void Relocate_(UnitPositionIndex& rPositions, Unit& rUnit, const CapturedUnit_t& rCaptured)
{
    if (rCaptured.pDestination == nullptr)
    {
        return;
    }
    rPositions.MoveUnit(rUnit, *rCaptured.pDestination);
    if (rCaptured.pCarrier != nullptr)
    {
        rUnit.Transport().EmbarkInto(*rCaptured.pCarrier);
    }
}

BaseManager* HomeBase_(Faction& rCaptor, const std::optional<BaseId_t>& rHomeBaseId)
{
    if (!rHomeBaseId)
    {
        return nullptr;
    }
    BaseManager* pBase = rCaptor.FindBase(*rHomeBaseId);
    if (pBase == nullptr)
    {
        throw std::logic_error("CaptureUnits: the home base is not the captor's");
    }
    return pBase;
}

void RestoreHp_(Unit& rUnit)
{
    const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
    rUnit.SetCurrentHp(maxHp);
}

void SetMoves_(Unit& rUnit, CapturedMoves_t moves)
{
    switch (moves)
    {
    case CapturedMoves_t::Spent:
        rUnit.SetMoveFragmentsRemaining(0);
        return;
    case CapturedMoves_t::Refilled:
    {
        const int fullMoves =
            rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
        rUnit.SetMoveFragmentsRemaining(fullMoves);
        return;
    }
    }
    throw std::logic_error("CaptureUnits: unknown CapturedMoves_t");
}

void CaptureUnit_(UnitPositionIndex& rPositions, Faction& rCaptor, const CapturedUnit_t& rCaptured)
{
    Unit& rUnit = *rCaptured.pUnit;
    ChangeOwner_(rUnit, rCaptor);
    Relocate_(rPositions, rUnit, rCaptured);
    rUnit.SetHomeBase(HomeBase_(rCaptor, rCaptured.homeBaseId));
    if (rCaptured.bRestoresHp)
    {
        RestoreHp_(rUnit);
    }
    SetMoves_(rUnit, rCaptured.moves);
    rUnit.ClearOrder();
}

const Unit& FindCaptured_(const Faction& rCaptor, UnitId_t unitId)
{
    for (const Unit& rUnit : rCaptor.GetUnitManager().Units())
    {
        if (rUnit.GetUnitId() == unitId)
        {
            return rUnit;
        }
    }
    throw std::logic_error("ApplyCombatCaptureAftermath: a captured unit is not the captor's");
}

CaptureNoticeInteraction_t MakeCaptureNotice_(const GameState& rGameState, const Faction& rCaptor,
                                              const CombatResult_t& rResult)
{
    const Faction& rVictim = rGameState.RequireFaction(rResult.defenderFactionId);
    const UnitId_t namedId =
        rResult.bDefenderBoarded ? rResult.defenderId : rResult.capturedUnitIds.front();
    CaptureNoticeInteraction_t notice;
    notice.kind = rResult.bDefenderBoarded ? CaptureNoticeKind_t::Boarded
                                           : CaptureNoticeKind_t::TakenOnDefeat;
    notice.captorId = rCaptor.GetFactionId();
    notice.victimId = rVictim.GetFactionId();
    notice.captorNoun = rCaptor.GetDefinition().identity.noun;
    notice.captorAdjective = rCaptor.GetDefinition().identity.adjective;
    notice.victimAdjective = rVictim.GetDefinition().identity.adjective;
    notice.unitName = FindCaptured_(rCaptor, namedId).GetDesign().GetName();
    notice.count = static_cast<int>(rResult.capturedUnitIds.size());
    notice.bVictimInVendetta =
        rGameState.GetDiplomacyLedger().HasVendetta(notice.captorId, notice.victimId);
    return notice;
}

void EnqueueCaptureNotice_(GameState& rGameState, const Faction& rCaptor,
                           const CombatResult_t& rResult)
{
    const Faction& rVictim = rGameState.RequireFaction(rResult.defenderFactionId);
    if (!rCaptor.IsPlayerControlled() && !rVictim.IsPlayerControlled())
    {
        return;
    }
    EnqueueForPlayer(rGameState, MakeCaptureNotice_(rGameState, rCaptor, rResult));
}

} // namespace

void CaptureUnits(UnitPositionIndex& rPositions, Faction& rCaptor,
                  const std::vector<CapturedUnit_t>& rCaptured)
{
    for (const CapturedUnit_t& rUnit : rCaptured)
    {
        CaptureUnit_(rPositions, rCaptor, rUnit);
    }
}

std::optional<UnitId_t> RemoveDefeatedUnit(UnitPositionIndex& rPositions, Unit& rAttacker,
                                           Unit& rDefeated)
{
    rAttacker.GetFaction().GetEconomy().AddEnergy(PlanetPearlsBounty(rDefeated));
    const std::optional<CapturedUnit_t> capture = DefeatedUnitCapture(rAttacker, rDefeated);
    if (!capture)
    {
        rDefeated.GetFaction().GetUnitManager().DestroyUnit(rDefeated);
        return std::nullopt;
    }
    CaptureUnits(rPositions, rAttacker.GetFaction(), {*capture});
    return rDefeated.GetUnitId();
}

void ApplyCombatCaptureAftermath(GameState& rGameState, const Unit& rAttacker,
                                 const CombatResult_t& rResult)
{
    const FactionId_t captorId = rAttacker.GetFaction().GetFactionId();
    const FactionId_t victimId = rResult.defenderFactionId;
    if (!rResult.bDefenderBoarded)
    {
        const int friction = ArtifactCaptureFriction(captorId, victimId);
        ApplyFriction(rGameState, victimId, captorId, friction);
    }
    EnqueueCaptureNotice_(rGameState, rAttacker.GetFaction(), rResult);
}

} // namespace ac
