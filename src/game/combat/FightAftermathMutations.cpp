#include "game/combat/FightAftermathMutations.h"

#include "game/GameState.h"
#include "game/base/BaseConquestMutations.h"
#include "game/combat/CaptureMutations.h"
#include "game/combat/CombatResolver.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/faction/Faction.h"
#include "game/units/Unit.h"

namespace ac
{

void ApplyFightConsequences(GameState& rGameState, Unit& rAttacker, const Tile& rBattleTile,
                            CombatResult_t& rResult)
{
    ApplyAlliedTerritoryFriction(rGameState, rAttacker.GetFaction().GetFactionId(),
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
