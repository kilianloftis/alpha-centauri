#pragma once

#include "game/faction/base/BaseTypes.h"
#include "lib/effects/ActiveEffect.h"
#include <memory>
#include <vector>

namespace ac
{

class WorkerAssignmentManager;
class EconomyManager;
class BuildingManager;
class Tile;
class TileEffectsContext;

// ResourceManager calculates resource production for a base and accumulates per-turn
// stockpiles. It is owned by BaseManager and holds const pointers to the managers it reads from.
class ResourceManager
{
public:
    ResourceManager(
        const WorkerAssignmentManager* pWorkerAssignments,
        const EconomyManager* pEconomy,
        const BuildingManager* pBuildings,
        const Tile* pBaseTile,
        const TileEffectsContext* pTileEffects);
    ~ResourceManager();

    // Resource production per turn.
    // rBaseEffects is this base's final effect list (BaseManager::BuildBaseEffects_).
    int GetNutrientProduction(const BaseEffects_t& rBaseEffects) const;
    int GetMineralProduction(const BaseEffects_t& rBaseEffects) const;
    int GetEconProduction(const BaseEffects_t& rBaseEffects) const;
    int GetLabsProduction(const BaseEffects_t& rBaseEffects) const;
    int GetPsychProduction(const BaseEffects_t& rBaseEffects) const;

    // Consume the full accumulated stockpile, returning the amount consumed.
    // Called by the appropriate turn stage (e.g. ConsumeMinerals during BaseProduction).
    int ConsumeNutrients();
    int ConsumeMinerals();
    int ConsumeEcon();
    int ConsumeLabs();
    int ConsumePsych();

    // Produce resources from worked tiles and allocate energy into stockpiles.
    // Called once per turn per base from the ResourceCollection stage.
    void ProduceResources(const BaseEffects_t& rBaseEffects);

private:
    const WorkerAssignmentManager* m_pWorkerAssignments;
    const EconomyManager* m_pEconomy;
    const BuildingManager* m_pBuildings;
    const Tile* m_pBaseTile;
    const TileEffectsContext* m_pTileEffects;
    int m_nutrients = 0;
    int m_minerals = 0;
    int m_econ = 0;
    int m_labs = 0;
    int m_psych = 0;

    TileResources_t ComputeWorked_(const BaseEffects_t& rBaseEffects) const;

    int CalculateResource_(StatId stat, const TileResources_t& worked, const BaseEffects_t& rBaseEffects) const;
    int CalculateEcon_(int energy, const BaseEffects_t& rBaseEffects) const;
    int CalculateLabs_(int energy, const BaseEffects_t& rBaseEffects) const;
    int CalculatePsych_(int energy, const BaseEffects_t& rBaseEffects) const;

    void ProduceNutrients_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
    void ProduceMinerals_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
    void AllocateEnergy_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
};

} // namespace ac
