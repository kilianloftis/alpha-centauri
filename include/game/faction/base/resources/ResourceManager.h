#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/effects/ActiveEffect.h"
#include <memory>
#include <vector>

namespace ac
{

class WorkerAssignmentManager;
class EconomyManager;
class BuildingManager;
class HomeBaseIndex;
class Tile;
class TileEffectsContext;

// ResourceManager calculates resource production for a base and accumulates per-turn
// stockpiles. It is owned by BaseManager and holds const pointers to the managers it reads from.
//
// Energy pipeline (per base, each turn):
//   1. Produce energy (tiles, crawlers, buildings, Energy StatModifiers)
//   2. Apply inefficiency (stub; will use HQ distance + efficiency rating)
//   3. Split into econ / labs / psych via the faction EconomyManager percentages
//   4. Apply Econ / Labs / Psych StatModifiers to each category
// Faction CollectIncome / CollectResearch take econ and labs; psych stays here for composition.
class ResourceManager
{
public:
    // All are owned by (or resolved from) the base that owns this manager, so none can be
    // absent. They were pointers re-checked at every single use, with "not set" throws that no
    // caller could ever trigger. (The BuildingManager parameter this used to take was stored
    // and never read.)
    ResourceManager(
        const WorkerAssignmentManager& rWorkerAssignments,
        const EconomyManager& rEconomy,
        const Tile& rBaseTile,
        const TileEffectsContext& rTileEffects,
        const HomeBaseIndex& rHomeUnits);
    ~ResourceManager();

    // Resource production per turn.
    // rBaseEffects is this base's final effect list (BaseManager::BuildBaseEffects_).
    int GetNutrientProduction(const BaseEffects_t& rBaseEffects) const;
    int GetMineralProduction(const BaseEffects_t& rBaseEffects) const;
    // Raw energy after Energy effects, before inefficiency.
    int GetEnergyProduction(const BaseEffects_t& rBaseEffects) const;
    int GetEconProduction(const BaseEffects_t& rBaseEffects) const;
    int GetLabsProduction(const BaseEffects_t& rBaseEffects) const;
    // Local psych% of post-inefficiency energy + Psych StatModifiers (facilities/specialists).
    int GetPsychProduction(const BaseEffects_t& rBaseEffects) const;

    // Consume the full accumulated stockpile, returning the amount consumed.
    // Called by the appropriate turn stage (e.g. ConsumeMinerals during BaseProduction).
    // ConsumePsych is for pop composition (not yet wired).
    int ConsumeNutrients();
    int ConsumeMinerals();
    int ConsumeEcon();
    int ConsumeLabs();
    int ConsumePsych();

    // Produce nutrients/minerals and allocate energy into econ/labs/psych stockpiles.
    // Called once per turn per base from the ResourceCollection stage.
    void ProduceResources(const BaseEffects_t& rBaseEffects);

    // Ownership transfer (BaseManager::RebindFaction): the energy allocation split (econ /
    // labs / psych percentages) is per-faction, so a transferred base must read the new
    // owner's EconomyManager from the next ProduceResources call.
    void RebindEconomy(const EconomyManager& rEconomy);

private:
    const WorkerAssignmentManager& m_rWorkerAssignments;
    // Re-pointed by RebindEconomy on ownership transfer; always the current owner's.
    const EconomyManager* m_pEconomy;
    const Tile& m_rBaseTile;
    const TileEffectsContext& m_rTileEffects;
    const HomeBaseIndex& m_rHomeUnits;
    int m_nutrients = 0;
    int m_minerals = 0;
    int m_econ = 0;
    int m_labs = 0;
    int m_psych = 0;

    TileResources_t ComputeWorked_(const BaseEffects_t& rBaseEffects) const;

    int CalculateResource_(StatId_t stat, const TileResources_t& worked, const BaseEffects_t& rBaseEffects) const;
    // Post-inefficiency energy used for the econ/labs/psych split.
    int AllocatableEnergy_(const BaseEffects_t& rBaseEffects) const;
    int ApplyInefficiency_(int energy) const;
    int CalculateEcon_(int energy, const BaseEffects_t& rBaseEffects) const;
    int CalculateLabs_(int energy, const BaseEffects_t& rBaseEffects) const;
    int CalculatePsych_(int energy, const BaseEffects_t& rBaseEffects) const;

    void ProduceNutrients_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
    void ProduceMinerals_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
    void AllocateEnergy_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects);
};

} // namespace ac
