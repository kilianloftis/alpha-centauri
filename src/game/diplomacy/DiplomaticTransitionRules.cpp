#include "game/diplomacy/DiplomaticTransitionRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/AttackRules.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/visibility/VisibilityRules.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

HostileAct_t HostileActAgainst(const Unit& rActor, const Unit& rTarget)
{
    const FactionId_t aggressor = rActor.GetFaction().GetFactionId();
    const FactionId_t victim = rTarget.GetFaction().GetFactionId();
    return {.aggressor = aggressor,
            .victim = victim,
            .bAttributed = IsOwnerKnownTo(victim, rActor) && IsOwnerKnownTo(aggressor, rTarget)};
}

HostileAct_t HostileActAgainst(const Unit& rActor, FactionId_t victim)
{
    return {.aggressor = rActor.GetFaction().GetFactionId(),
            .victim = victim,
            .bAttributed = IsOwnerKnownTo(victim, rActor)};
}

bool IsHostileAttack(const Unit& rAttacker, const Unit& rDefender)
{
    EffectContext_t ctx{&rDefender.GetTile(), CombatRole_t::Defender};
    ctx.pAttacker = &rAttacker;
    ctx.pDefender = &rDefender;
    return !ResolveFlag(rDefender, RuleFlagId_t::AttackNotHostile, ctx);
}

std::optional<HostileAct_t> AttackHostileAct(const Unit& rAttacker, const Unit& rDefender)
{
    if (!IsHostileAttack(rAttacker, rDefender))
    {
        return std::nullopt;
    }
    return HostileActAgainst(rAttacker, rDefender);
}

std::optional<VendettaKind_t> VendettaFromHostileAct(const HostileAct_t& rAct)
{
    if (!rAct.bAttributed || StatusRulesFor(rAct.aggressor, rAct.victim).bMayAttack)
    {
        return std::nullopt;
    }
    return VendettaKind_t::SneakAttack;
}

std::optional<DiplomaticStatus_t> StatusBrokenByAttack(const Unit& rAttacker,
                                                       const Unit& rDefender)
{
    const std::optional<HostileAct_t> act = AttackHostileAct(rAttacker, rDefender);
    const DiplomacyLedger& rLedger = CurrentGameState().GetDiplomacyLedger();
    if (!act || !VendettaFromHostileAct(*act) || !rLedger.AreKnown(act->aggressor, act->victim))
    {
        return std::nullopt;
    }
    return rLedger.GetStatus(act->aggressor, act->victim);
}

bool MayBreakByAttack(DiplomaticStatus_t broken)
{
    return CurrentGameState().GetGameData().diplomacyConfig->For(broken).bBreakByAttack;
}

bool VendettaWithdrawsTerritory(VendettaKind_t kind)
{
    return kind == VendettaKind_t::Declaration;
}

bool HonoredObligationCascades()
{
    switch (CurrentGameState().GetGameData().diplomacyConfig->defensiveObligationMode)
    {
    case DefensiveObligationMode_t::JoinAsDefender:
        return false;
    case DefensiveObligationMode_t::SeparateDeclaration:
        return true;
    }
    throw std::logic_error("HonoredObligationCascades: unknown defensive obligation mode");
}

std::vector<HostileAct_t> BombardHostileActs(const Unit& rAttacker, const Tile& rTarget)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    std::vector<HostileAct_t> acts;
    const auto addAct = [&](const HostileAct_t& rAct)
    {
        if (rAct.victim == rAct.aggressor || rAct.victim == k_NoFactionOwner)
        {
            return;
        }
        const auto it = std::find_if(acts.begin(), acts.end(), [&](const HostileAct_t& rKnown)
                                     { return rKnown.victim == rAct.victim; });
        if (it == acts.end())
        {
            acts.push_back(rAct);
        }
        else
        {
            it->bAttributed = it->bAttributed || rAct.bAttributed;
        }
    };
    if (const Unit* pDuelTarget = ArtilleryDuelTarget(rAttacker, rTarget))
    {
        addAct(HostileActAgainst(rAttacker, *pDuelTarget));
    }
    for (const Unit* pTarget : BombardStrikeTargets(rAttacker, rTarget))
    {
        if (IsHostileAttack(rAttacker, *pTarget))
        {
            addAct(HostileActAgainst(rAttacker, *pTarget));
        }
    }
    if (!BombardImprovementTargets(rTarget).empty())
    {
        const FactionId_t owner = rWorldMap.GetTerritory().GetOwner(rTarget);
        addAct(HostileActAgainst(rAttacker, owner));
    }
    return acts;
}

bool CanProposeStepUp(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    return rLedger.AreKnown(a, b) && StepUp(rLedger.GetStatus(a, b)).has_value();
}

bool CanDeclareVendetta(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!rLedger.AreKnown(a, b))
    {
        return false;
    }
    return rLedger.GetStatus(a, b) != DiplomaticStatus_t::Vendetta;
}

bool CanCancelTreaty(const DiplomacyLedger& rLedger, FactionId_t a, FactionId_t b)
{
    if (!rLedger.AreKnown(a, b))
    {
        return false;
    }
    return IsAgreement(rLedger.GetStatus(a, b));
}

bool IsAgreement(DiplomaticStatus_t status)
{
    return StepDown(status).has_value();
}

int AgreementBreakBlemishes(DiplomaticStatus_t from)
{
    if (!IsAgreement(from))
    {
        return 0;
    }
    return CurrentGameState().GetGameData().diplomacyConfig->breakAgreementBlemishes;
}

std::optional<FactionId_t> AlliedTerritoryOwner(FactionId_t attacker, FactionId_t defender,
                                                const Tile& rBattleTile)
{
    // TODO: SMAC's other territory site, +3 against a faction that refused the defender's demand
    // to attack (0x5078BB), waits on AI-initiated #DEMANDATTACK.
    const GameState& rGame = CurrentGameState();
    const FactionId_t owner = rGame.GetWorldMap().GetTerritory().GetOwner(rBattleTile);
    if (owner == k_NoFactionOwner || owner == attacker || owner == defender)
    {
        return std::nullopt;
    }
    const DiplomacyLedger& rLedger = rGame.GetDiplomacyLedger();
    if (!rLedger.HasVendetta(owner, defender) || !IsAgreement(rLedger.GetStatus(owner, attacker)))
    {
        return std::nullopt;
    }
    return owner;
}

} // namespace ac
