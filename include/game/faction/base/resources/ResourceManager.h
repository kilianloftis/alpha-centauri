#pragma once

#include "game/faction/base/BaseTypes.h"
#include <functional>
#include <memory>
#include <vector>

namespace ac
{

class Tile;
class PopContainer;
class PopulationManager;
class WorkerAssignmentManager;

// ResourceManager handles resource production, stockpiling, buildings, and trade routes.
// It is owned by BaseManager and receives PopulationManager and WorkerAssignmentManager
// references to calculate resource production from worked tiles.
class ResourceManager
{
public:
    ResourceManager(PopulationManager* pPopulation, WorkerAssignmentManager* pWorkerAssignments);
    ~ResourceManager();

    // Tile lookup used by resource calculations. Must be set before calling
    // GetNutrientProduction() / GetEnergyProduction() / GetMineralProduction().
    // If not set, all tile-derived resource values return 0.
    void SetTileLookup(std::function<const Tile*(int x, int y)> tileLookup);

    // Trade route management
    void AddTradeRoute(const TradeRoute_t& tradeRoute);
    void RemoveTradeRoute(int targetFactionId);
    const std::vector<TradeRoute_t>& GetTradeRoutes() const;

    // Resource production calculation
    int GetNutrientProduction() const;
    int GetEnergyProduction() const;
    int GetMineralProduction() const;

    // Stockpile accessors.
    // Note: Nutrient stockpile is separate from the GrowthCalculator bank.
    //       The stockpile is for spending (e.g., rushing production).
    int GetNutrientStockpile() const;
    int GetMineralStockpile() const;

    // Accumulate this turn's production into base stockpiles (nutrients, minerals).
    // Call once per turn from the appropriate turn stage (BaseProduction).
    // Note: Energy is not accumulated here - it flows directly to faction-level allocation.
    void AccumulateStockpiles();

    // Consume resources from stockpiles. Returns actual amount consumed.
    int ConsumeNutrients(int amount);
    int ConsumeMinerals(int amount);

    // Building management
    void AddBuilding(const std::string& buildingId);
    void RemoveBuilding(const std::string& buildingId);
    const std::vector<std::string>& GetBuildings() const;

private:
    PopulationManager* m_pPopulation;
    WorkerAssignmentManager* m_pWorkerAssignments;
    std::function<const Tile*(int x, int y)> m_tileLookup;
    std::vector<std::string> m_buildings;
    std::vector<TradeRoute_t> m_tradeRoutes;
    int m_nutrientStockpile;
    int m_mineralStockpile;

    int CalculateNutrients_() const;
    int CalculateEnergyProduction_() const;
    int CalculateMinerals_() const;
};

} // namespace ac
