#pragma once

#include "lib/Signal.h"
#include <memory>
#include <vector>
#include <string>

namespace ac
{

using FactionId = int;

class PopulationManager;

struct TradeRoute_t
{
    int energyBonus;
    int targetFactionId;
    int targetBaseId;
};

struct TileResources_t
{
    int nutrients;
    int energy;
    int minerals;
};

class Base
{
public:
    Base();
    ~Base();

    // Population management
    PopulationManager* GetPopulation();
    const PopulationManager* GetPopulation() const;
    void AddPop();

    // Building management
    void AddBuilding(const std::string& buildingId);
    void RemoveBuilding(const std::string& buildingId);
    const std::vector<std::string>& GetBuildings() const;

    // Resource management from worked tiles
    void SetWorkedTiles(const TileResources_t& resources);
    TileResources_t GetWorkedTiles() const;

    // Trade route management
    void AddTradeRoute(const TradeRoute_t& tradeRoute);
    void RemoveTradeRoute(int targetFactionId);
    const std::vector<TradeRoute_t>& GetTradeRoutes() const;

    int GetNutrientProduction() const;
    int GetEnergyProduction() const;
    
    // Base identity
    void SetName(const std::string& name);
    const std::string& GetName() const;

    // Ownership
    void SetFactionId(FactionId factionId);
    FactionId GetFactionId() const;
    void SetBaseId(int baseId);
    int GetBaseId() const;

private:
    FactionId m_factionId;
    int m_baseId;
    int CalculateNutrients_() const;
    int CalculateEnergyProduction_() const;
    int CalculateMinerals_() const;

    void ApplyProduction_();
    void ApplyNutrition_();
    void ApplyPsych_();
    
    std::unique_ptr<PopulationManager> m_pPopulation;
    std::vector<std::string> m_buildings;
    TileResources_t m_workedTiles;
    std::vector<TradeRoute_t> m_tradeRoutes;
    std::string m_name;
};

} // namespace ac
