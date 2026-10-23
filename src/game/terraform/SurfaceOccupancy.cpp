#include "game/terraform/SurfaceOccupancy.h"

#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/terraform/TerraformRules.h"
#include "game/CurrentGameState.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/buildings/BuildingManager.h"
#include "game/population/PopulationManager.h"
#include "game/units/UnitManager.h"

#include <optional>
#include <random>
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

// The player hears of their own flooded base, or of one they can see (BASESUBMERGED popups).
void NoticeSubmergence_(GameState& rGameState, const BaseManager& rBase, bool bDestroyed)
{
    const Faction& rPlayer = rGameState.GetPlayerFaction();
    const bool bOwn = &rBase.GetFaction() == &rPlayer;
    if (!bOwn && !rPlayer.GetVisibleMap().IsVisible(rBase.GetTile()))
    {
        return;
    }
    const std::string whose = bOwn ? "" : rBase.GetFaction().GetDefinition().identity.name + " ";
    const std::string body = bDestroyed
        ? "The " + whose + "base " + rBase.GetName() + " has been lost beneath the rising sea."
        : "The " + whose + "base " + rBase.GetName()
              + " has been submerged. Its population has been reduced, and it now has a "
                "Pressure Dome.";
    const TileCoord_t at{rBase.GetTile().GetX(), rBase.GetTile().GetY()};
    EnqueueForPlayer(rGameState, NoticeInteraction_t{std::nullopt, "Base Submerged", body, at});
}

// Returns whether the base still stands.
bool FloodBase_(GameState& rGameState, BaseManager& rBase, const SubmergedBase_t& rSubmerged)
{
    std::uniform_int_distribution<int> rollDistribution(rSubmerged.rollMin, rSubmerged.rollMax);
    const int roll = rollDistribution(rGameState.GetRng());
    const SubmergenceOutcome_t outcome = ResolveSubmergence(rSubmerged, roll);
    NoticeSubmergence_(rGameState, rBase, outcome.bDestroyed);
    if (outcome.bDestroyed)
    {
        rBase.GetFaction().RazeBase(rBase);
        return false;
    }
    for (int i = 0; i < outcome.popLoss; ++i)
    {
        rBase.GetPopulation().RemovePop();
    }
    rBase.GetBuildingManager().AddBuilding(k_PressureDomeBuildingId);
    return true;
}

void DestroyUnits_(const std::vector<Unit*>& rUnits)
{
    for (Unit* pUnit : rUnits)
    {
        pUnit->GetFaction().GetUnitManager().DestroyUnit(*pUnit);
    }
}

void ReconcileTile_(GameState& rGameState, const SurfaceFlip_t& rFlip)
{
    Tile& rTile = *rFlip.pTile;
    BaseManager* pBase = rGameState.FindBaseAt(rTile.GetX(), rTile.GetY());
    const SurfaceFlipConsequences_t consequences =
        SurfaceFlipConsequences(rTile, rFlip.bNowWater, pBase);
    bool bBaseStands = pBase != nullptr;
    if (consequences.submerged)
    {
        bBaseStands = FloodBase_(rGameState, *pBase, *consequences.submerged);
    }
    RemoveImprovements_(rGameState.GetTileEffects(), rTile, consequences.removedImprovements);
    if (!bBaseStands)
    {
        DestroyUnits_(consequences.strandedUnits);
    }
}

} // namespace

void ReconcileSurfaceFlips(GameState& rGameState, const std::vector<SurfaceFlip_t>& rFlips)
{
    RequireCurrentGameState(rGameState);
    for (const SurfaceFlip_t& rFlip : rFlips)
    {
        ReconcileTile_(rGameState, rFlip);
    }
}

} // namespace ac
