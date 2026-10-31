#include "game/diplomacy/DiplomaticStandingMutations.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/faction/Faction.h"
#include "game/terraform/TerraformRules.h"

#include <optional>

namespace ac
{

void ApplyFriction(GameState& rGameState, FactionId_t holder, FactionId_t against, int delta)
{
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomacyCalculator& rCalculator = *rGameState.GetGameData().diplomacyCalculator;
    const int current = rLedger.GetFriction(holder, against);
    rLedger.SetFriction(holder, against, rCalculator.NextFriction(current, delta));
}

void ApplyBlemishes(GameState& rGameState, FactionId_t faction, BlemishChange_t change)
{
    DiplomacyLedger& rLedger = rGameState.GetDiplomacyLedger();
    const DiplomacyCalculator& rCalculator = *rGameState.GetGameData().diplomacyCalculator;
    const int current = rLedger.GetBlemishes(faction);
    const int next = rCalculator.NextBlemishes(current, change.delta, change.floor);
    rLedger.SetBlemishes(faction, next);
}

void ApplyAlliedTerritoryFriction(GameState& rGameState, FactionId_t attacker,
                                  FactionId_t defender, const Tile& rBattleTile)
{
    const std::optional<FactionId_t> owner = AlliedTerritoryOwner(attacker, defender, rBattleTile);
    if (!owner)
    {
        return;
    }
    const int friction = rGameState.GetGameData().diplomacyConfig->alliedTerritoryBattleFriction;
    ApplyFriction(rGameState, *owner, attacker, friction);
}

void ApplyRainfallGrievance(GameState& rGameState, const Faction& rTerraformer,
                            std::span<const MoistureChange_t> changes)
{
    const std::optional<FactionId_t> grievant = RainfallGrievant(rTerraformer, changes);
    if (!grievant)
    {
        return;
    }
    const int friction = rGameState.GetGameData().diplomacyConfig->terraformRainfallFriction;
    ApplyFriction(rGameState, *grievant, rTerraformer.GetFactionId(), friction);
}

} // namespace ac
