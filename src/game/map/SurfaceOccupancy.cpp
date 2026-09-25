#include "game/map/SurfaceOccupancy.h"

#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/units/IUnitOrderWorld.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

bool DomainMatches_(const ImprovementConfig_t& rConfig, bool bNowWater)
{
    if (!rConfig.domain.has_value())
    {
        return true;
    }
    const bool bDomainWater = *rConfig.domain == ImprovementDomain_t::Sea;
    return bDomainWater == bNowWater;
}

std::vector<std::string> ImprovementsToRemove_(const Tile& rTile, bool bNowWater, bool bSpareBase)
{
    std::vector<std::string> remove;
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (!pImprovement || DomainMatches_(*pImprovement, bNowWater))
        {
            continue;
        }
        if (bSpareBase && pImprovement->id == ImprovementIds::k_Base)
        {
            continue;
        }
        remove.push_back(pImprovement->id);
    }
    return remove;
}

void SettleSubmergedBase_(BaseManager* pBase, bool bNowWater, IUnitOrderWorld* pWorld,
                          const std::vector<std::string>& rRemoving)
{
    if (!bNowWater)
    {
        return;
    }
    if (pBase && !pBase->MayOccupyWater())
    {
        pBase->GetFaction().RazeBase(*pBase);
        return;
    }
    if (pWorld)
    {
        return;
    }
    for (const std::string& rId : rRemoving)
    {
        if (rId == ImprovementIds::k_Base)
        {
            throw std::logic_error(
                "ReconcileSurfaceFlips: a base tile became water with no world to raze it");
        }
    }
}

void RemoveImprovements_(TileEffectsContext& rTileEffects, Tile& rTile,
                         const std::vector<std::string>& rIds)
{
    for (const std::string& rId : rIds)
    {
        rTileEffects.RemoveImprovementWithEffects(rTile, rId);
    }
}

void ReconcileTile_(TileEffectsContext& rTileEffects, IUnitOrderWorld* pWorld,
                    const SurfaceFlip_t& rFlip)
{
    if (!rFlip.pTile)
    {
        return;
    }
    Tile& rTile = *rFlip.pTile;
    BaseManager* pBase = pWorld ? pWorld->FindBaseAt(rTile.GetX(), rTile.GetY()) : nullptr;
    const bool bSpareBase = rFlip.bNowWater && pBase && pBase->MayOccupyWater();
    const std::vector<std::string> remove =
        ImprovementsToRemove_(rTile, rFlip.bNowWater, bSpareBase);
    SettleSubmergedBase_(pBase, rFlip.bNowWater, pWorld, remove);
    RemoveImprovements_(rTileEffects, rTile, remove);
}

} // namespace

void ReconcileSurfaceFlips(TileEffectsContext& rTileEffects, IUnitOrderWorld* pWorld,
                           const std::vector<SurfaceFlip_t>& rFlips)
{
    for (const SurfaceFlip_t& rFlip : rFlips)
    {
        ReconcileTile_(rTileEffects, pWorld, rFlip);
    }
}

} // namespace ac
