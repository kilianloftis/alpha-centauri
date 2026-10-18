#include "game/diplomacy/DiplomaticTransitionRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/combat/AttackRules.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/faction/Faction.h"
#include "game/map/TerritoryMap.h"
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

std::optional<VendettaKind_t> VendettaFromHostileAct(const HostileAct_t& rAct)
{
    if (!rAct.bAttributed || StatusRulesFor(rAct.aggressor, rAct.victim).bMayAttack)
    {
        return std::nullopt;
    }
    return VendettaKind_t::SneakAttack;
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

std::vector<HostileAct_t> BombardHostileActs(const Unit& rAttacker, const Tile& rTarget,
                                             const WorldMap& rWorldMap,
                                             const TileEffectsContext& rTileEffects)
{
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
    if (const Unit* pDuelTarget = ArtilleryDuelTarget(rAttacker, rTarget, rWorldMap, rTileEffects))
    {
        addAct(HostileActAgainst(rAttacker, *pDuelTarget));
    }
    for (const Unit* pTarget : BombardStrikeTargets(rAttacker, rTarget, rWorldMap, rTileEffects))
    {
        addAct(HostileActAgainst(rAttacker, *pTarget));
    }
    if (!BombardImprovementTargets(rTarget, rWorldMap).empty())
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
    return StepDown(rLedger.GetStatus(a, b)).has_value();
}

} // namespace ac
