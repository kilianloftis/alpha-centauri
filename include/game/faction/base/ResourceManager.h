#pragma once

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BaseTypes.h"
#include "game/faction/base/WorkerAssignmentManager.h"
#include <functional>
#include <vector>

namespace ac
{

class Tile;
class PopContainer;

// ResourceManager is a subclass of BaseManager that handles resource
// production, stockpiling, buildings, and trade routes.
// This extracts the resource-related functionality from the old Base class.
class ResourceManager : public BaseManager
{
public:
    ResourceManager();
    ~ResourceManager() override;

    // Override to return self as resource manager
    ResourceManager* GetResourceManager() override;
    const ResourceManager* GetResourceManager() const override;

    // Building management
    void AddBuilding(const std::string& buildingId);
    void RemoveBuilding(const std::string& buildingId);
    const std::vector<std::string>& GetBuildings() const;

    // Tile lookup used by resource calculations. Must be set before calling
    // GetNutrientProduction() / GetEnergyProduction() / GetMineralProduction().
    // If not set, all tile-derived resource values return 0.
    void SetTileLookup(WorkerAssignmentManager::TileLookup tileLookup);

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

private:
    WorkerAssignmentManager::TileLookup m_tileLookup;
    std::vector<std::string> m_buildings;
    std::vector<TradeRoute_t> m_tradeRoutes;
    int m_nutrientStockpile;
    int m_mineralStockpile;

    int CalculateNutrients_() const;
    int CalculateEnergyProduction_() const;
    int CalculateMinerals_() const;
};

} // namespace ac
