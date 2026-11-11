#pragma once

#include "game/base/BaseConquestMutations.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/base/BaseTypes.h"
#include "game/combat/CombatResolver.h"
#include "game/supply/SupplyPodMutations.h"

#include <optional>
#include <random>

namespace ac
{

class BaseManager;
class Tile;
class Unit;
struct TerraformProject_t;

// The session queries and hooks UnitOrderExecutor and UnitAttackExecutor need beyond their map
// and pathfinder.
class IUnitOrderWorld
{
public:
    virtual ~IUnitOrderWorld() = default;

    virtual BaseManager* FindBaseAt(int tileX, int tileY) = 0;

    virtual std::optional<CombatResult_t> TryInterceptAttack(
        Unit& rAttacker, Unit& rDefender, std::mt19937& rRng) = 0;

    virtual BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover, std::mt19937& rRng) = 0;

    // rVisitor arrived on a tile with on_visit_effects.
    virtual void OnImprovementVisited(Unit& rVisitor) = 0;

    // Opens a supply pod on rVisitor's tile.
    virtual SupplyPodOpenResult_t ApplySupplyPodOpen(Unit& rVisitor, std::mt19937& rRng) = 0;

    // A unit's attack, described before it resolved. Called once it has resolved.
    virtual void OnHostileAct(const HostileAct_t& rAct) = 0;

    // ApplyFightConsequences after a fight or duel on rBattleTile, never a bombard strike.
    virtual void ApplyFightConsequences(Unit& rAttacker, const Tile& rBattleTile,
                                        CombatResult_t& rResult) = 0;

    // A player unit's move order reached the border of territoryOwner's territory, which its
    // faction may not enter. The order is kept until the player decides.
    virtual void OnTerritoryEntryRefused(Unit& rUnit, FactionId_t territoryOwner) = 0;

    // Fires the project's on_complete_effects against the Former's tile. Returns whether any
    // effect applied.
    virtual bool ApplyTerraformCompleteEffects(Tile& rTile, const TerraformProject_t& rProject,
                                               Unit& rFormer, std::mt19937& rRng) = 0;
};

} // namespace ac
