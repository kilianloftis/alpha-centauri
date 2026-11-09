#include "game/base/FoundBaseRules.h"

#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"
#include "game/units/UnitTransport.h"

#include <vector>

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

bool SurfaceMatchesDomain_(const Tile& rTile, UnitDomain_t domain)
{
    if (rTile.IsWater())
    {
        // Sea founds on water; land / air / orbital do not.
        return domain == UnitDomain_t::Sea;
    }
    return true;
}

} // namespace

bool IsTooCloseToAnyBase(const Tile& rTile, const std::vector<const BaseManager*>& rBases)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    const int mapWidth = rWorldMap.GetWidth();
    for (const BaseManager* pBase : rBases)
    {
        if (!pBase)
        {
            continue;
        }
        if (ChebyshevDistance(rTile, pBase->GetTile(), mapWidth) < k_MinBaseFoundingSeparation)
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

bool CanFoundBaseAt(const Tile& rTile, FactionId_t founderFactionId,
                    const std::vector<const BaseManager*>& rBases)
{
    if (IsTooCloseToAnyBase(rTile, rBases))
    {
        return false;
    }
    const GameState& rGame = CurrentGameState();
    const WorldMap& rWorldMap = rGame.GetWorldMap();
    if (IsInForeignTerritory(rTile, founderFactionId, rWorldMap.GetTerritory()))
    {
        return false;
    }
    return true;
}

bool CanFoundBase(const Unit& rUnit)
{
    if (!UnitCanFoundBase(rUnit))
    {
        return false;
    }
    if (rUnit.Transport().IsEmbarked())
    {
        return false;
    }
    const Tile& rTile = rUnit.GetTile();
    if (!SurfaceMatchesDomain_(rTile, rUnit.GetDomain()))
    {
        return false;
    }
    const GameState& rGame = CurrentGameState();
    const std::vector<const BaseManager*> bases = CollectAllBases_(rGame);
    return CanFoundBaseAt(rTile, rUnit.GetFaction().GetFactionId(), bases);
}

} // namespace ac
