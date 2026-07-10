#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/BonusEffect.h"

namespace ac
{

namespace
{

int GetResourceValue_(const TileResources_t& resources, StatId_t resourceId)
{
    switch (resourceId)
    {
        case StatId_t::Nutrients: return resources.nutrients;
        case StatId_t::Minerals: return resources.minerals;
        case StatId_t::Energy: return resources.energy;
        default: return 0;
    }
}

} // namespace

ResourceManager::ResourceManager(
    const WorkerAssignmentManager* pWorkerAssignments,
    const EconomyManager* pEconomy,
    const BuildingManager* pBuildings,
    const Tile* pBaseTile,
    const TileEffectsContext* pTileEffects)
    : m_pWorkerAssignments(pWorkerAssignments)
    , m_pEconomy(pEconomy)
    , m_pBuildings(pBuildings)
    , m_pBaseTile(pBaseTile)
    , m_pTileEffects(pTileEffects)
{
}

ResourceManager::~ResourceManager()
{
}

// Total worked resources: every worker pop's fully-modified tile yield (from
// WorkerAssignmentManager) plus the base center tile, which is worked for free (no pop).
TileResources_t ResourceManager::ComputeWorked_(const BaseEffects_t& rBaseEffects) const
{
    if (!m_pWorkerAssignments || !m_pTileEffects)
    {
        throw std::runtime_error("WorkerAssignmentManager or TileEffectsContext not set");
    }
    TileResources_t total = m_pWorkerAssignments->ComputeWorkedResources(rBaseEffects);
    if (m_pBaseTile)
    {
        const TileResources_t baseTile = m_pTileEffects->ResolveTileYield(*m_pBaseTile, /*isBaseTile*/true, rBaseEffects);
        total.nutrients += baseTile.nutrients;
        total.energy    += baseTile.energy;
        total.minerals  += baseTile.minerals;
    }
    return total;
}

int ResourceManager::CalculateResource_(StatId_t stat, const TileResources_t& worked,
                                        const BaseEffects_t& rBaseEffects) const
{
    // Per-tile yield modifiers are already folded into `worked`; only flat (non-selector)
    // stat modifiers remain to be applied once at the base level.
    double base = static_cast<double>(GetResourceValue_(worked, stat));
    base += ResolveStatModifiers(FilterFlatByStatId(rBaseEffects, stat), SeedFor(stat)).total;
    return static_cast<int>(base);
}

int ResourceManager::GetNutrientProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateResource_(StatId_t::Nutrients, ComputeWorked_(rBaseEffects), rBaseEffects);
}

int ResourceManager::GetMineralProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateResource_(StatId_t::Minerals, ComputeWorked_(rBaseEffects), rBaseEffects);
}

int ResourceManager::CalculateEcon_(int energy, const BaseEffects_t& rBaseEffects) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    // FilterFlat, not FilterByStatId: base-level resolution must never pick up
    // selector-carrying (per-tile) modifiers, even on stats where none make sense today.
    return m_pEconomy->CalculateEnergyForEcon(energy)
         + static_cast<int>(ResolveStatModifiers(FilterFlatByStatId(rBaseEffects, StatId_t::Econ), SeedFor(StatId_t::Econ)).total);
}

int ResourceManager::CalculateLabs_(int energy, const BaseEffects_t& rBaseEffects) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForLabs(energy)
         + static_cast<int>(ResolveStatModifiers(FilterFlatByStatId(rBaseEffects, StatId_t::Labs), SeedFor(StatId_t::Labs)).total);
}

int ResourceManager::CalculatePsych_(int energy, const BaseEffects_t& rBaseEffects) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForPsych(energy)
         + static_cast<int>(ResolveStatModifiers(FilterFlatByStatId(rBaseEffects, StatId_t::Psych), SeedFor(StatId_t::Psych)).total);
}

int ResourceManager::GetEconProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateEcon_(CalculateResource_(StatId_t::Energy, ComputeWorked_(rBaseEffects), rBaseEffects), rBaseEffects);
}

int ResourceManager::GetLabsProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateLabs_(CalculateResource_(StatId_t::Energy, ComputeWorked_(rBaseEffects), rBaseEffects), rBaseEffects);
}

int ResourceManager::GetPsychProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculatePsych_(CalculateResource_(StatId_t::Energy, ComputeWorked_(rBaseEffects), rBaseEffects), rBaseEffects);
}

int ResourceManager::ConsumeNutrients()
{
    int consumed = m_nutrients;
    m_nutrients = 0;
    return consumed;
}

int ResourceManager::ConsumeMinerals()
{
    int consumed = m_minerals;
    m_minerals = 0;
    return consumed;
}

int ResourceManager::ConsumeEcon()
{
    int consumed = m_econ;
    m_econ = 0;
    return consumed;
}

int ResourceManager::ConsumeLabs()
{
    int consumed = m_labs;
    m_labs = 0;
    return consumed;
}

int ResourceManager::ConsumePsych()
{
    int consumed = m_psych;
    m_psych = 0;
    return consumed;
}

void ResourceManager::ProduceNutrients_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects)
{
    m_nutrients += CalculateResource_(StatId_t::Nutrients, worked, rBaseEffects);
}

void ResourceManager::ProduceMinerals_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects)
{
    m_minerals += CalculateResource_(StatId_t::Minerals, worked, rBaseEffects);
}

void ResourceManager::AllocateEnergy_(const TileResources_t& worked, const BaseEffects_t& rBaseEffects)
{
    const int energy = CalculateResource_(StatId_t::Energy, worked, rBaseEffects);

    m_econ  += CalculateEcon_(energy, rBaseEffects);
    m_labs  += CalculateLabs_(energy, rBaseEffects);
    m_psych += CalculatePsych_(energy, rBaseEffects);
}

void ResourceManager::ProduceResources(const BaseEffects_t& rBaseEffects)
{
    const TileResources_t worked = ComputeWorked_(rBaseEffects);
    ProduceNutrients_(worked, rBaseEffects);
    ProduceMinerals_(worked, rBaseEffects);
    AllocateEnergy_(worked, rBaseEffects);
}

} // namespace ac
