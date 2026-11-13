#include "game/combat/FightAftermathRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/map/TerritoryMap.h"
#include "game/map/WorldMap.h"

namespace ac
{

std::optional<FactionId_t> AlliedTerritoryOwner(FactionId_t attacker, FactionId_t defender,
                                                const Tile& rBattleTile)
{
    // TODO: AI motivations required: SMAC's other territory site, +3 against a faction that refused the defender's demand
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
