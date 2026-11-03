#include "game/diplomacy/BaseValueRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/base/BaseRules.h"
#include "game/buildings/BuildingConfig.h"
#include "game/buildings/BuildingManager.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyConfig.h"
#include "game/economy/EconomyManager.h"
#include "game/faction/Faction.h"
#include "game/map/MapUtils.h"
#include "game/map/RegionCache.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/PopulationManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace ac
{

namespace
{

// What one base found by the scan does to the distance, and whether the scan goes on.
struct NeighbourStep_t
{
    int distanceDelta = 0;
    bool bContinue = true;
};

NeighbourStep_t NeighbourStep_(const BaseManager& rNear, bool bSameRegion,
                               const Faction& rEvaluator, const Faction& rCounterparty)
{
    const Faction& rOwner = rNear.GetFaction();
    if (&rOwner == &rEvaluator)
    {
        return {bSameRegion ? -1 : 0, false};
    }
    if (!bSameRegion)
    {
        return {0, true};
    }
    if (&rOwner == &rCounterparty)
    {
        return {1, true};
    }
    return {0, false};
}

int ScannedDistance_(const BaseManager& rBase, const Faction& rEvaluator,
                     const Faction& rCounterparty, int hqDistance)
{
    const GameState& rGame = CurrentGameState();
    const WorldMap& rMap = rGame.GetWorldMap();
    const RegionCache& rRegions = rMap.GetRegions();
    const int region = rRegions.RegionOf(rBase.GetTile());
    const int radius = rGame.GetGameData().diplomacyConfig->baseValue.scanRadius;
    int distance = hqDistance;
    ForEachTileInRings(rBase.GetTile(), rMap, radius, [&](const Tile* pTile)
    {
        const BaseManager* pNear = rGame.FindBaseAt(pTile->GetX(), pTile->GetY());
        if (!pNear)
        {
            return true;
        }
        const bool bSameRegion = rRegions.RegionOf(*pTile) == region;
        const NeighbourStep_t step = NeighbourStep_(*pNear, bSameRegion, rEvaluator, rCounterparty);
        distance += step.distanceDelta;
        return step.bContinue;
    });
    return distance;
}

int LargerTreasury_(const Faction& rEvaluator, const Faction& rCounterparty)
{
    return std::max(rEvaluator.GetEconomy().GetEnergy(), rCounterparty.GetEconomy().GetEnergy());
}

// buildings.json follows alphax.txt #FACILITIES, so registry order is SMAC's facility id order.
std::vector<int> FacilityMinerals_(const BaseManager& rBase)
{
    const BuildingRegistry& rRegistry = *CurrentGameState().GetGameData().buildingRegistry;
    const BuildingManager& rBuildings = rBase.GetBuildingManager();
    std::vector<int> minerals;
    for (const BuildingConfig_t& rConfig : rRegistry.GetAll())
    {
        if (!rConfig.bIsSecretProject && !rConfig.orbital && rBuildings.HasBuilding(rConfig.id))
        {
            minerals.push_back(rConfig.mineralCost);
        }
    }
    return minerals;
}

int ProjectMinerals_(const BaseManager& rBase)
{
    int total = 0;
    for (const BuildingConfig_t* pConfig : rBase.GetBuildingManager().GetBuildings())
    {
        if (pConfig->bIsSecretProject)
        {
            total += pConfig->mineralCost;
        }
    }
    return total;
}

int UnitMinerals_(const std::vector<Unit*>& rUnits, const Faction& rOwner)
{
    int total = 0;
    for (const Unit* pUnit : rUnits)
    {
        if (&pUnit->GetFaction() == &rOwner)
        {
            total += pUnit->GetDesign().GetBaseCost();
        }
    }
    return total;
}

} // namespace

int BaseValue(const BaseManager& rBase, const Faction& rEvaluator, const Faction& rCounterparty)
{
    const GameState& rGame = CurrentGameState();
    const int hqDistance = HeadquartersDistance(rEvaluator, rBase.GetTile());
    if (hqDistance <= 0)
    {
        throw std::logic_error("BaseValue: '" + rBase.GetName()
                               + "' is the evaluator's headquarters");
    }
    const int region = rGame.GetWorldMap().GetRegions().RegionOf(rBase.GetTile());
    const std::vector<Unit*> units = rGame.GetWorldMap().GetAllUnitsOnTile(rBase.GetTile());

    BaseValueInputs_t inputs;
    inputs.distance = ScannedDistance_(rBase, rEvaluator, rCounterparty, hqDistance);
    inputs.credits = LargerTreasury_(rEvaluator, rCounterparty);
    inputs.population = rBase.GetPopulation().GetSize();
    inputs.bGarrisoned = !units.empty();
    inputs.evaluatorRegionBases = RegionBaseCount(rEvaluator, region);
    inputs.counterpartyRegionBases = RegionBaseCount(rCounterparty, region);
    inputs.facilityMinerals = FacilityMinerals_(rBase);
    inputs.projectMinerals = ProjectMinerals_(rBase);
    inputs.evaluatorUnitMinerals = UnitMinerals_(units, rEvaluator);
    // TODO: the binary also halves for the AI's fear, quadruples for its hostility and for a
    // scenario objective, and doubles for a BELIEVE evaluator; we have none of those inputs.
    return rGame.GetGameData().diplomacyCalculator->BaseValue(inputs);
}

} // namespace ac
