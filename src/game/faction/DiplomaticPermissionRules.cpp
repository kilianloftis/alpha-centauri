#include "game/faction/DiplomaticPermissionRules.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/Unit.h"
#include "game/map/TerritoryMap.h"

#include <stdexcept>

namespace ac
{

namespace
{

const GameState& SessionOf_(const Faction& rFaction)
{
    const GameState* pState = rFaction.GetGameState();
    if (!pState)
    {
        throw std::logic_error(
            "Diplomatic permission rules: faction is not attached to a game session");
    }
    return *pState;
}

} // namespace

const DiplomaticStatusRules_t& StatusRulesFor(const GameState& rGameState, FactionId_t a,
                                              FactionId_t b)
{
    return rGameState.GetGameData().diplomacyConfig->For(
        rGameState.GetDiplomacyLedger().GetStatus(a, b));
}

bool HasDiplomacy(const Faction& rFaction)
{
    return !IsNativeLifeFaction(rFaction.GetDefinition().identity.species);
}

bool IsObligedToDefend(const GameState& rGameState, FactionId_t partner, FactionId_t ally,
                       FactionId_t aggressor)
{
    if (partner == ally || partner == aggressor)
    {
        return false;
    }
    return StatusRulesFor(rGameState, partner, ally).bDefensiveObligation
        && !rGameState.GetDiplomacyLedger().HasVendetta(partner, aggressor);
}

bool MayEnterTerritoryOf(const Unit& rGuest, FactionId_t territoryOwner)
{
    const Faction& rGuestFaction = rGuest.GetFaction();
    const FactionId_t guestId = rGuestFaction.GetFactionId();
    if (territoryOwner == k_NoFactionOwner || territoryOwner == guestId
        || !IsOwnerKnownTo(territoryOwner, rGuest))
    {
        return true;
    }
    return StatusRulesFor(SessionOf_(rGuestFaction), guestId, territoryOwner).bEnterTerritory;
}

bool MayRepairAt(const Unit& rUnit, const BaseManager& rBase)
{
    const Faction& rGuest = rUnit.GetFaction();
    const FactionId_t hostId = rBase.GetFaction().GetFactionId();
    if (rGuest.GetFactionId() == hostId)
    {
        return true;
    }
    return StatusRulesFor(SessionOf_(rGuest), rGuest.GetFactionId(), hostId).bRepairAtBases;
}

bool MayShareTiles(const Faction& rA, FactionId_t b)
{
    if (rA.GetFactionId() == b)
    {
        return true;
    }
    return StatusRulesFor(SessionOf_(rA), rA.GetFactionId(), b).bShareTiles;
}

} // namespace ac
