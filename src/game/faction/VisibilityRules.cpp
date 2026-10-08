#include "game/faction/VisibilityRules.h"

#include "game/Faction.h"
#include "game/GameSettings.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/FactionVisibleMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

namespace ac
{

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

void ApplyRemoveFog(Faction& rFaction)
{
    rFaction.SetFogRemoved(true);
    rFaction.GetVisibleMap().SetRemoveFog(true);
    rFaction.GetVisibleMap().MarkAll();
}

void ApplyVisibilityRules(Faction& rFaction, const GameSettings& rSettings)
{
    const bool bRemoveFog =
        rFaction.IsFogRemoved()
        || ResolveFlag(rFaction, RuleFlagId_t::RemoveFog)
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
