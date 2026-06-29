#pragma once

#include "game/faction/base/BaseTypes.h"
#include "lib/effects/ActiveEffect.h"
#include <memory>
#include <vector>

namespace ac
{

class PopulationManager;
class WorkerAssignmentManager;
class EconomyManager;
class BuildingManager;
class Tile;

// ResourceManager calculates and caches resource production for a base.
// It is owned by BaseManager and holds const pointers to the managers it reads from.
class ResourceManager
{
public:
    ResourceManager(
        const PopulationManager* pPopulation,
        const WorkerAssignmentManager* pWorkerAssignments,
        const EconomyManager* pEconomy,
        const BuildingManager* pBuildings,
        const Tile* pBaseTile);
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
    void ProduceResources(const std::vector<ActiveEffect_t>& activeEffects);

private:
    const PopulationManager* m_pPopulation;
    const WorkerAssignmentManager* m_pWorkerAssignments;
    const EconomyManager* m_pEconomy;
    const BuildingManager* m_pBuildings;
    const Tile* m_pBaseTile;
    std::vector<ActiveEffect_t> m_activeEffects;
    int m_nutrients = 0;
    int m_minerals = 0;
    int m_econ = 0;
    int m_labs = 0;
    int m_psych = 0;

    int CalculateNutrients_(const std::vector<ActiveEffect_t>& activeEffects) const;
    int CalculateMinerals_(const std::vector<ActiveEffect_t>& activeEffects) const;
    int CalculateEnergy_(const std::vector<ActiveEffect_t>& activeEffects) const;

    void ProduceNutrients_(const std::vector<ActiveEffect_t>& activeEffects);
    void ProduceMinerals_(const std::vector<ActiveEffect_t>& activeEffects);
    void AllocateEnergy_(const std::vector<ActiveEffect_t>& activeEffects);
    void ProduceResourcesInternal_(const std::vector<ActiveEffect_t>& activeEffects);
};

} // namespace ac
