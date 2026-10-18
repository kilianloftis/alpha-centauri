#include "game/terraform/SurfaceOccupancy.h"

#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/terraform/TerraformRules.h"
#include "game/units/IUnitOrderWorld.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

void RemoveImprovements_(TileEffectsContext& rTileEffects, Tile& rTile,
                         const std::vector<std::string>& rIds)
{
    for (const std::string& rId : rIds)
    {
        rTileEffects.RemoveOccupantWithEffects(rTile, rId);
    }
}

void ReconcileTile_(TileEffectsContext& rTileEffects, IUnitOrderWorld* pWorld,
                    const SurfaceFlip_t& rFlip)
{
    Tile& rTile = *rFlip.pTile;
    BaseManager* pBase = pWorld ? pWorld->FindBaseAt(rTile.GetX(), rTile.GetY()) : nullptr;
    const SurfaceFlipConsequences_t consequences =
        SurfaceFlipConsequences(rTile, rFlip.bNowWater, pBase);
    const std::vector<std::string>& rRemoved = consequences.removedImprovements;
    if (consequences.bRazesBase)
    {
        pBase->GetFaction().RazeBase(*pBase);
    }
    else if (rFlip.bNowWater && pWorld == nullptr
             && std::find(rRemoved.begin(), rRemoved.end(), ImprovementIds::k_Base)
                    != rRemoved.end())
    {
        throw std::logic_error(
            "ReconcileSurfaceFlips: a base tile became water with no world to raze it");
    }
    RemoveImprovements_(rTileEffects, rTile, rRemoved);
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
