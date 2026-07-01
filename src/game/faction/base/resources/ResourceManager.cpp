#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "lib/effects/ActiveEffect.h"
#include "lib/effects/TileEffectsContext.h"
#include "lib/effects/BonusEffect.h"

namespace ac
{

namespace
{

int GetResourceValue_(const TileResources_t& resources, StatId resourceId)
{
    switch (resourceId)
    {
        case StatId::Nutrients: return resources.nutrients;
        case StatId::Minerals: return resources.minerals;
        case StatId::Energy: return resources.energy;
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
TileResources_t ResourceManager::ComputeWorked_() const
{
    if (!m_pWorkerAssignments || !m_pTileEffects)
    {
        throw std::runtime_error("WorkerAssignmentManager or TileEffectsContext not set");
    }
    TileResources_t total = m_pWorkerAssignments->ComputeWorkedResources(m_activeEffects);
    if (m_pBaseTile)
    {
        const TileResources_t baseTile = m_pTileEffects->ResolveTileYield(*m_pBaseTile, /*isBaseTile*/true, m_activeEffects);
        total.nutrients += baseTile.nutrients;
        total.energy    += baseTile.energy;
        total.minerals  += baseTile.minerals;
    }
    return total;
}

int ResourceManager::CalculateResource_(StatId stat, const TileResources_t& worked) const
{
    // Per-tile yield modifiers are already folded into `worked`; only flat (non-selector)
    // stat modifiers remain to be applied once at the base level.
    double base = static_cast<double>(GetResourceValue_(worked, stat));
    base += ResolveStatModifiers(FilterFlatByStatId(m_activeEffects, stat)).total;
    return static_cast<int>(base);
}

int ResourceManager::GetNutrientProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateResource_(StatId::Nutrients, ComputeWorked_());
}

int ResourceManager::GetMineralProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateResource_(StatId::Minerals, ComputeWorked_());
}

int ResourceManager::CalculateEcon_(int energy) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForEcon(energy)
         + static_cast<int>(ResolveStatModifiers(FilterByStatId(m_activeEffects, StatId::Econ)).total);
}

int ResourceManager::CalculateLabs_(int energy) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForLabs(energy)
         + static_cast<int>(ResolveStatModifiers(FilterByStatId(m_activeEffects, StatId::Labs)).total);
}

int ResourceManager::CalculatePsych_(int energy) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForPsych(energy)
         + static_cast<int>(ResolveStatModifiers(FilterByStatId(m_activeEffects, StatId::Psych)).total);
}

int ResourceManager::GetEconProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateEcon_(CalculateResource_(StatId::Energy, ComputeWorked_()));
}

int ResourceManager::GetLabsProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateLabs_(CalculateResource_(StatId::Energy, ComputeWorked_()));
}

int ResourceManager::GetPsychProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculatePsych_(CalculateResource_(StatId::Energy, ComputeWorked_()));
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

void ResourceManager::ProduceNutrients_(const TileResources_t& worked)
{
    m_nutrients += CalculateResource_(StatId::Nutrients, worked);
}

void ResourceManager::ProduceMinerals_(const TileResources_t& worked)
{
    m_minerals += CalculateResource_(StatId::Minerals, worked);
}

void ResourceManager::AllocateEnergy_(const TileResources_t& worked)
{
    const int energy = CalculateResource_(StatId::Energy, worked);

    m_econ  += CalculateEcon_(energy);
    m_labs  += CalculateLabs_(energy);
    m_psych += CalculatePsych_(energy);
}

void ResourceManager::ProduceResources(const std::vector<ActiveEffect_t>& activeEffects)
{
    m_activeEffects = activeEffects;

    const TileResources_t worked = ComputeWorked_();
    ProduceNutrients_(worked);
    ProduceMinerals_(worked);
    AllocateEnergy_(worked);
}

} // namespace ac
