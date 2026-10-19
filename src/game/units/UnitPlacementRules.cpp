#include "game/units/UnitPlacementRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitDomain.h"

namespace ac
{

namespace
{

// CanHoldTileWithoutCarrier needs a live unit, which a grant does not have yet. A tile that
// harbors the domain for this faction always works; otherwise its surface has to suit it.
bool DesignCanHoldTile_(const UnitDesign& rDesign, const Tile& rTile, FactionId_t factionId,
                        const WorldMap& rWorldMap)
{
    const UnitDomain_t domain = rDesign.GetDomain();
    if (TileHarbors(rTile, domain, factionId, rWorldMap.GetTerritory()))
    {
        return true;
    }
    switch (domain)
    {
        case UnitDomain_t::Land:
            return !rTile.IsWater();
        case UnitDomain_t::Sea:
            return rTile.IsWater();
        case UnitDomain_t::Air:
        case UnitDomain_t::Orbital:
            return true;
    }
    return false;
}

} // namespace

std::optional<BaseId_t> GrantAnchorBase(const Faction& rFaction, const BaseManager* pContextBase,
                                        const Tile* pContextTile)
{
    if (pContextBase != nullptr && &pContextBase->GetFaction() == &rFaction)
    {
        return pContextBase->GetBaseId();
    }
    if (pContextTile != nullptr)
    {
        if (const BaseManager* pNearest = rFaction.FindNearestBase(*pContextTile))
        {
            return pNearest->GetBaseId();
        }
    }
    if (const BaseManager* pHq = rFaction.GetHeadquarters())
    {
        return pHq->GetBaseId();
    }
    for (const BaseManager& rBase : rFaction.Bases())
    {
        return rBase.GetBaseId();
    }
    return std::nullopt;
}

std::vector<const Tile*> PlacementTiles(const Tile& rOrigin, const UnitDesign& rDesign,
                                        FactionId_t factionId, int needed)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const UnitPositionIndex& rPositions = rWorldMap.GetUnitPositions();
    std::vector<const Tile*> found;
    const auto consider = [&](const Tile* pTile)
    {
        if (!CanPlaceUnitOnTile(*pTile, rPositions)
            || !DesignCanHoldTile_(rDesign, *pTile, factionId, rWorldMap))
        {
            return;
        }
        found.push_back(pTile);
    };

    consider(&rOrigin);
    // The hardcoded 2 is terrible design, but I think we'll get rid of single-tile mode
    for (int radius = 1; found.size() < static_cast<size_t>(needed) && radius <= 2; ++radius)
    {
        ForEachTileInChebyshevRadius(rOrigin, rWorldMap, radius, /*includeOrigin=*/false,
            [&](const Tile* pTile, int distance)
            {
                if (distance == radius)
                {
                    consider(pTile);
                }
            });
    }
    return found;
}

} // namespace ac
