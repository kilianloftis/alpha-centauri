#include "game/movement/EvacuateTerritoryRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"
#include "game/visibility/VisibilityRules.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace ac
{

namespace
{

bool HoldsUnitItMayNotShareWith_(const Unit& rUnit, const Tile& rTile, const WorldMap& rWorldMap)
{
    for (const Unit* pOther : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pOther && !MayShareTiles(rUnit.GetFaction(), pOther->GetFaction().GetFactionId()))
        {
            return true;
        }
    }
    return false;
}

// A unit already on its own territory stays; whoever shares the tile with it moves instead.
bool IsValidEvacuateDestination_(const Unit& rUnit, const Tile& rTile,
                                 const WorldMap& rWorldMap, FactionId_t ownFactionId)
{
    if (rWorldMap.GetTerritory().GetOwner(rTile) != ownFactionId)
    {
        return false;
    }
    if (!CanHoldTileWithoutCarrier(rUnit, rTile))
    {
        return false;
    }
    if (&rTile != &rUnit.GetTile() && HoldsUnitItMayNotShareWith_(rUnit, rTile, rWorldMap))
    {
        return false;
    }
    return CanPlaceUnitOnTile(rTile, rWorldMap.GetUnitPositions());
}

bool IsBetterEvacuateTieBreak_(const Tile& rCandidate, int bestY, int bestX)
{
    const int y = rCandidate.GetY();
    const int x = rCandidate.GetX();
    return y < bestY || (y == bestY && x < bestX);
}

// Best valid tile on the Chebyshev ring at `distance` (Y then X). nullptr if none.
const Tile* FindBestOnChebyshevRing_(const Unit& rUnit, const Tile& rOrigin,
                                     const WorldMap& rWorldMap, FactionId_t ownFactionId,
                                     int distance)
{
    const Tile* pBest = nullptr;
    int bestY = std::numeric_limits<int>::max();
    int bestX = std::numeric_limits<int>::max();

    auto consider = [&](int p, int q)
    {
        const Tile* pTile = GetTileAtLatticeOffset(rWorldMap, rOrigin, p, q);
        if (!pTile || !IsValidEvacuateDestination_(rUnit, *pTile, rWorldMap, ownFactionId))
        {
            return;
        }
        if (!pBest || IsBetterEvacuateTieBreak_(*pTile, bestY, bestX))
        {
            bestY = pTile->GetY();
            bestX = pTile->GetX();
            pBest = pTile;
        }
    };

    if (distance == 0)
    {
        consider(0, 0);
        return pBest;
    }

    for (int q = -distance; q <= distance; ++q)
    {
        for (int p = -distance; p <= distance; ++p)
        {
            if (std::max(std::abs(p), std::abs(q)) != distance)
            {
                continue;
            }
            consider(p, q);
        }
    }
    return pBest;
}

int MaxChebyshevEvacuateRadius_(const WorldMap& rWorldMap)
{
    return (rWorldMap.GetWidth() / 2 + std::max(0, rWorldMap.GetHeight() - 1)) / 2;
}

} // namespace

bool IsOnTerritoryOf(const Tile& rTile, FactionId_t hostTerritoryOwner,
                     const TerritoryMap& rTerritory)
{
    return rTerritory.GetOwner(rTile) == hostTerritoryOwner;
}

bool MustLeaveTerritory(const Unit& rUnit, FactionId_t hostTerritoryOwner,
                        const TerritoryMap& rTerritory)
{
    return IsOnTerritoryOf(rUnit.GetTile(), hostTerritoryOwner, rTerritory)
           && IsOwnerKnownTo(hostTerritoryOwner, rUnit);
}

bool SharesTileWith(const Unit& rUnit, const Faction& rHost)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const Tile& rTile = rUnit.GetTile();
    for (const BaseManager& rBase : rHost.Bases())
    {
        if (&rBase.GetTile() == &rTile)
        {
            return true;
        }
    }
    for (const Unit* pOther : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (&pOther->GetFaction() == &rHost)
        {
            return true;
        }
    }
    return false;
}

const Tile* FindNearestOwnTerritoryTile(const Unit& rUnit)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const FactionId_t ownId = rUnit.GetFaction().GetFactionId();
    const Tile& rOrigin = rUnit.GetTile();
    const int maxRadius = MaxChebyshevEvacuateRadius_(rWorldMap);

    for (int distance = 0; distance <= maxRadius; ++distance)
    {
        if (const Tile* pBest =
                FindBestOnChebyshevRing_(rUnit, rOrigin, rWorldMap, ownId, distance))
        {
            return pBest;
        }
    }
    return nullptr;
}

} // namespace ac
