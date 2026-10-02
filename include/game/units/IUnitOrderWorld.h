#pragma once

#include "game/units/BaseConquestEffects.h"
#include "game/faction/base/BaseTypes.h"
#include "game/units/CombatResolver.h"

#include <optional>
#include <random>

namespace ac
{

class BaseManager;
class Tile;
class TileEffectsContext;
class Unit;

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

    // aggressor's unit is about to attack something of victim's.
    virtual void OnHostileAct(FactionId_t aggressor, FactionId_t victim) = 0;

    // A player unit's move order reached the border of territoryOwner's territory, which its
    // faction may not enter. The order is kept until the player decides.
    virtual void OnTerritoryEntryRefused(Unit& rUnit, FactionId_t territoryOwner) = 0;
};

} // namespace ac
