#include "game/faction/DiplomaticPermissionRules.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/CurrentGameState.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/UnitVisibility.h"
#include "game/faction/base/BaseManager.h"
#include "game/units/Unit.h"
#include "game/map/TerritoryMap.h"

#include <stdexcept>

namespace ac
{

const DiplomaticStatusRules_t& StatusRulesFor(FactionId_t a, FactionId_t b)
{
    const GameState& rGameState = CurrentGameState();
    const DiplomaticStatus_t status = rGameState.GetDiplomacyLedger().GetStatus(a, b);
    return rGameState.GetGameData().diplomacyConfig->For(status);
}

bool HasDiplomacy(const Faction& rFaction)
{
    return !IsNativeLifeFaction(rFaction.GetDefinition().identity.species);
}

bool IsObligedToDefend(FactionId_t partner, FactionId_t ally, FactionId_t aggressor)
{
    const GameState& rGameState = CurrentGameState();
    if (partner == ally || partner == aggressor)
    {
        return false;
    }
    return StatusRulesFor(partner, ally).bDefensiveObligation
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
    return StatusRulesFor(guestId, territoryOwner).bEnterTerritory;
}

bool MayRepairAt(const Unit& rUnit, const BaseManager& rBase)
{
    const Faction& rGuest = rUnit.GetFaction();
    const FactionId_t hostId = rBase.GetFaction().GetFactionId();
    if (rGuest.GetFactionId() == hostId)
    {
        return true;
    }
    return StatusRulesFor(rGuest.GetFactionId(), hostId).bRepairAtBases;
}

bool MayShareTiles(const Faction& rA, FactionId_t b)
{
    if (rA.GetFactionId() == b)
    {
        return true;
    }
    return StatusRulesFor(rA.GetFactionId(), b).bShareTiles;
}

} // namespace ac
