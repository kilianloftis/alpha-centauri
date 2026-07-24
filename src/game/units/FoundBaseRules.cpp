#include "game/units/FoundBaseRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

namespace ac
{

namespace
{

std::vector<const BaseManager*> CollectAllBases_(const GameState& rGameState)
{
    std::vector<const BaseManager*> bases;
    for (const Faction& rFaction : rGameState.Factions())
    {
        for (const BaseManager& rBase : rFaction.Bases())
        {
            bases.push_back(&rBase);
        }
    }
    return bases;
}

} // namespace

bool IsTooCloseToAnyBase(const Tile& rTile, const WorldMap& rWorldMap,
                         const std::vector<const BaseManager*>& rBases)
{
    const int mapWidth = rWorldMap.GetWidth();
    for (const BaseManager* pBase : rBases)
    {
        if (!pBase)
        {
            continue;
        }
        const Tile* pBaseTile = rWorldMap.GetTile(pBase->GetX(), pBase->GetY());
        if (!pBaseTile)
        {
            continue;
        }
        if (ChebyshevDistance(rTile, *pBaseTile, mapWidth) < k_MinBaseFoundingSeparation)
        {
            return true;
        }
    }
    return false;
}

bool IsInForeignTerritory(const Tile& rTile, FactionId_t founderFactionId,
                          const TerritoryMap& rTerritory)
{
    const FactionId_t owner = rTerritory.GetOwner(rTile);
    return owner != k_NoFactionOwner && owner != founderFactionId;
}

bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const WorldMap& rWorldMap,
                    const std::vector<const BaseManager*>& rBases)
{
    if (IsTooCloseToAnyBase(rTile, rWorldMap, rBases))
    {
        return false;
    }
    if (IsInForeignTerritory(rTile, founderFactionId, rWorldMap.GetTerritory()))
    {
        return false;
    }
    return true;
}

bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId, const GameState& rGameState)
{
    return CanFoundBaseAt(rTile, founderFactionId, rGameState.GetWorldMap(),
                          CollectAllBases_(rGameState));
}

} // namespace ac
