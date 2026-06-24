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
class BaseEconomyManager;
class BuildingManager;

// ResourceManager calculates and caches resource production for a base.
// It is owned by BaseManager and holds const pointers to the managers it reads from.
class ResourceManager
{
public:
    ResourceManager(
        const PopulationManager* pPopulation,
        const WorkerAssignmentManager* pWorkerAssignments,
        const BaseEconomyManager* pEconomy,
        const BuildingManager* pBuildings,
        std::function<const Tile*(int x, int y)> tileLookup);
    ~ResourceManager();

    // Resource production per turn (calculated live from current state).
    // Safe to call at any time; used by the UI for display and estimates.
    int GetNutrientProduction() const;
    int GetMineralProduction() const;
    int GetEconProduction() const;
    int GetLabsProduction() const;
    int GetPsychProduction() const;

    // Consume the full accumulated stockpile, returning the amount consumed.
    // Called by the appropriate turn stage (e.g. ConsumeMinerals during BaseProduction).
    int ConsumeNutrients();
    int ConsumeMinerals();
    int ConsumeEcon();
    int ConsumeLabs();
    int ConsumePsych();

    // Produce resources from worked tiles and allocate energy into stockpiles.
    // Called once per turn per base from the ResourceCollection stage.
    void CollectResources();

private:
    const PopulationManager* m_pPopulation;
    const WorkerAssignmentManager* m_pWorkerAssignments;
    const BaseEconomyManager* m_pEconomy;
    const BuildingManager* m_pBuildings;
    std::function<const Tile*(int x, int y)> m_tileLookup;
    int m_nutrients = 0;
    int m_minerals = 0;
    int m_econ = 0;
    int m_labs = 0;
    int m_psych = 0;

    int CalculateNutrients_() const;
    int CalculateMinerals_() const;
    int CalculateEnergy_() const;

    void ProduceNutrients_();
    void ProduceMinerals_();
    void AllocateEnergy_();
    void ProduceResources_();
};

} // namespace ac
