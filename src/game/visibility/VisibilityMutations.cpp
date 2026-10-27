#include "game/visibility/VisibilityMutations.h"

#include "game/faction/Faction.h"
#include "game/GameSettings.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/visibility/FactionVisibleMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <vector>

namespace ac
{

bool CopyExploredMap(Faction& rDest, const Faction& rSource)
{
    const FactionExploredMap& rSourceExplored = rSource.GetExploredMap();
    FactionExploredMap& rDestExplored = rDest.GetExploredMap();
    std::vector<const Tile*> newlySeen;
    for (const auto& pOwnedTile : rDest.GetWorldMap().GetTiles())
    {
        if (!pOwnedTile)
        {
            continue;
        }
        if (rSourceExplored.IsExplored(*pOwnedTile) && !rDestExplored.IsExplored(*pOwnedTile))
        {
            newlySeen.push_back(pOwnedTile.get());
        }
    }
    if (newlySeen.empty())
    {
        return false;
    }
    rDestExplored.MergeFrom(rSourceExplored);
    // TODO: confirm in terranx.exe what a faction sees on tiles explored by map trade or shroud removal
    for (const Tile* pTile : newlySeen)
    {
        rDest.GetTileMemory().Record(*pTile);
    }
    return true;
}

void ApplyRemoveShroud(Faction& rFaction)
{
    FactionExploredMap& rExplored = rFaction.GetExploredMap();
    for (const auto& pOwnedTile : rFaction.GetWorldMap().GetTiles())
    {
        if (pOwnedTile && !rExplored.IsExplored(*pOwnedTile))
        {
            rFaction.GetTileMemory().Record(*pOwnedTile);
        }
    }
    // TODO: confirm in terranx.exe what a faction sees on tiles explored by map trade or shroud removal
    rExplored.MarkAll();
}

void ApplyVisibilityRules(Faction& rFaction, const GameSettings& rSettings)
{
    const bool bRemoveFog =
        ResolveFlag(rFaction, RuleFlagId_t::RemoveFog)
        || (rFaction.IsPlayerControlled() && rSettings.GetVisibility().removeFog);
    rFaction.GetVisibleMap().SetRemoveFog(bRemoveFog);
    if (bRemoveFog)
    {
        rFaction.GetVisibleMap().MarkAll();
    }

    const bool bRemoveShroud =
        ResolveFlag(rFaction, RuleFlagId_t::RemoveShroud)
        || rSettings.GetVisibility().removeShroud;
    if (bRemoveShroud)
    {
        ApplyRemoveShroud(rFaction);
    }
}

} // namespace ac
