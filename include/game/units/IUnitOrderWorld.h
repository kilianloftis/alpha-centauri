#pragma once

#include "game/units/BaseConquestEffects.h"
#include "game/units/CombatResolver.h"

#include <optional>
#include <random>

namespace ac
{

class BaseManager;
class Tile;
class TileEffectsContext;
class Unit;
struct GameDataContext;

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
        Unit& rAttacker, const Tile& rDefenderTile, const GameDataContext& rDataContext,
        std::mt19937& rRng) = 0;

    virtual BaseConquestResult_t ResolveBaseEntryConquest(
        Unit& rMover, const GameDataContext& rDataContext, std::mt19937& rRng) = 0;
};

} // namespace ac
