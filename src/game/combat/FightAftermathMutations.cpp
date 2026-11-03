#include "game/combat/FightAftermathMutations.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseConquestMutations.h"
#include "game/combat/CaptureMutations.h"
#include "game/combat/CombatResolver.h"
#include "game/combat/FightAftermathRules.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"

#include <optional>

namespace ac
{

namespace
{

// AlliedTerritoryOwner's faction gains allied_territory_battle_friction toward attacker, and
// attacker's gift count toward it rises to allied_territory_battle_gifts_floor.
void ApplyAlliedTerritoryStanding_(GameState& rGameState, FactionId_t attacker,
                                   FactionId_t defender, const Tile& rBattleTile)
{
    const std::optional<FactionId_t> owner = AlliedTerritoryOwner(attacker, defender, rBattleTile);
    if (!owner)
    {
        return;
    }
    const DiplomacyConfig_t& rConfig = *rGameState.GetGameData().diplomacyConfig;
    ApplyFriction(rGameState, *owner, attacker, rConfig.alliedTerritoryBattleFriction);
    RaiseGifts(rGameState, attacker, *owner, rConfig.alliedTerritoryBattleGiftsFloor);
}

} // namespace

void ApplyFightConsequences(GameState& rGameState, Unit& rAttacker, const Tile& rBattleTile,
                            CombatResult_t& rResult)
{
    ApplyAlliedTerritoryStanding_(rGameState, rAttacker.GetFaction().GetFactionId(),
                                  rResult.defenderFactionId, rBattleTile);
    // Only a surviving attacker takes units.
    if (!rResult.capturedUnitIds.empty())
    {
        ApplyCombatCaptureAftermath(rGameState, rAttacker, rResult);
    }
    // Combat never moves either side, so capturing an emptied base is a later step onto it. A
    // native raider spends itself on the raid, so playback must not show it surviving.
    if (rResult.bDefenderDestroyed
        && ResolvePostCombatBaseConquest(rAttacker, rBattleTile, rGameState, rGameState.GetRng())
               .bActorDestroyed)
    {
        rResult.bAttackerDestroyed = true;
    }
}

} // namespace ac
