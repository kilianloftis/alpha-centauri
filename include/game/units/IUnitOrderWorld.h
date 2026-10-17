#pragma once

#include "game/base/BaseConquestEffects.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/base/BaseTypes.h"
#include "game/combat/CombatResolver.h"

#include <optional>
#include <random>

namespace ac
{

class BaseManager;
class Tile;
class TileEffectsContext;
class Unit;
struct TerraformProject_t;

// Narrow session surface UnitOrderExecutor needs beyond the map / pathfinder it already
// holds. GameState implements it; movement-only harnesses construct the executor with a
// null world, which disables intercept and base conquest.
class IUnitOrderWorld
{
public:
    virtual ~IUnitOrderWorld() = default;

    virtual BaseManager* FindBaseAt(int tileX, int tileY) = 0;

    virtual std::optional<CombatResult_t> TryInterceptAttack(
        Unit& rAttacker, Unit& rDefender, TileEffectsContext& rTileEffects,
        std::mt19937& rRng) = 0;

    virtual BaseConquestResult_t ResolvePostCombatBaseConquest(
        Unit& rAttacker, const Tile& rDefenderTile, std::mt19937& rRng) = 0;

    virtual BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover, std::mt19937& rRng) = 0;

    // A unit's attack, described before it resolved. Called once it has resolved.
    virtual void OnHostileAct(const HostileAct_t& rAct) = 0;

    // A player unit's move order reached the border of territoryOwner's territory, which its
    // faction may not enter. The order is kept until the player decides.
    virtual void OnTerritoryEntryRefused(Unit& rUnit, FactionId_t territoryOwner) = 0;

    // Fires the project's on_complete_effects against the Former's tile. Returns whether any
    // effect applied.
    virtual bool ApplyTerraformCompleteEffects(Tile& rTile, const TerraformProject_t& rProject,
                                               Unit& rFormer, std::mt19937& rRng) = 0;
};

} // namespace ac
