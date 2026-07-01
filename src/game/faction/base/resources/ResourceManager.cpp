#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "lib/effects/ActiveEffect.h"
#include "lib/effects/TileEffectsContext.h"
#include "lib/effects/BonusEffect.h"
#include <algorithm>
#include <iterator>
#include <set>

namespace ac
{

namespace
{

std::vector<ActiveEffect_t> FilterTileYieldModifiersBySelector_(
    const std::vector<ActiveEffect_t>& effects,
    StatId resourceId,
    TileSelectorKind selectorKind)
{
    std::vector<ActiveEffect_t> matching;
    std::copy_if(effects.begin(), effects.end(), std::back_inserter(matching),
                 [resourceId, selectorKind](const ActiveEffect_t& effect)
                 {
                     if (!effect.config)
                     {
                         return false;
                     }
                     const TileYieldModifierEffect_t* pTileModifier = std::get_if<TileYieldModifierEffect_t>(&effect.config->effect);
                     return pTileModifier && pTileModifier->resource == resourceId &&
                            pTileModifier->selector.kind == selectorKind;
                 });
    return matching;
}

std::vector<ActiveEffect_t> FilterTileYieldModifiersBySelectorAndImprovement_(
    const std::vector<ActiveEffect_t>& effects,
    StatId resourceId,
    const std::string& improvementId)
{
    std::vector<ActiveEffect_t> matching;
    std::copy_if(effects.begin(), effects.end(), std::back_inserter(matching),
                 [resourceId, &improvementId](const ActiveEffect_t& effect)
                 {
                     if (!effect.config)
                     {
                         return false;
                     }
                     const TileYieldModifierEffect_t* pTileModifier = std::get_if<TileYieldModifierEffect_t>(&effect.config->effect);
                     if (!pTileModifier || pTileModifier->resource != resourceId ||
                         pTileModifier->selector.kind != TileSelectorKind::HasImprovement)
                     {
                         return false;
                     }
                     return pTileModifier->selector.improvement &&
                            *pTileModifier->selector.improvement == improvementId;
                 });
    return matching;
}

// Collects the unique improvement ids referenced by HasImprovement TileYieldModifier effects.
// Used to iterate only improvements that any active building effect cares about.
std::set<std::string> CollectHasImprovementIds_(const std::vector<ActiveEffect_t>& effects)
{
    std::set<std::string> ids;
    for (const ActiveEffect_t& effect : effects)
    {
        if (!effect.config) continue;
        const TileYieldModifierEffect_t* p = std::get_if<TileYieldModifierEffect_t>(&effect.config->effect);
        if (p && p->selector.kind == TileSelectorKind::HasImprovement && p->selector.improvement)
        {
            ids.insert(*p->selector.improvement);
        }
    }
    return ids;
}

TileResources_t ComputeBaseTileResources_(const Tile* pBaseTile, const TileEffectsContext& rTileEffects)
{
    TileResources_t resources = {};
    if (pBaseTile)
    {
        resources = rTileEffects.ResolveTileYield(*pBaseTile);
    }
    return resources;
}

TileResources_t ComputeTileResourcesForPop_(const Tile& tile, const Pop& pop, const TileEffectsContext& rTileEffects)
{
    const TileResources_t raw = rTileEffects.ResolveTileYield(tile);
    return pop.ApplyTileMultipliers(raw);
}

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

int CountWorkedTilesWithImprovement_(
    const WorkerAssignmentManager& workerAssignments,
    const PopContainer& pops,
    const std::string& improvementId)
{
    int count = 0;
    for (const auto& pPop : pops.GetPops())
    {
        if (!pPop || !pPop->IsWorker())
        {
            continue;
        }
        const Tile* pTile = pPop->GetTile();
        if (!pTile)
        {
            continue;
        }
        if (!workerAssignments.IsTileAssigned(pTile))
        {
            continue;
        }
        if (pTile->HasImprovement(improvementId))
        {
            ++count;
        }
    }
    return count;
}

int ComputeWorkedResourceForImprovement_(
    const WorkerAssignmentManager& workerAssignments,
    const PopContainer& pops,
    const std::string& improvementId,
    StatId resourceId,
    const TileEffectsContext& rTileEffects)
{
    int total = 0;
    for (const auto& pPop : pops.GetPops())
    {
        if (!pPop || !pPop->IsWorker())
        {
            continue;
        }
        const Tile* pTile = pPop->GetTile();
        if (!pTile)
        {
            continue;
        }
        if (!workerAssignments.IsTileAssigned(pTile))
        {
            continue;
        }
        if (!pTile->HasImprovement(improvementId))
        {
            continue;
        }
        total += GetResourceValue_(ComputeTileResourcesForPop_(*pTile, *pPop, rTileEffects), resourceId);
    }
    return total;
}

double ResolveTileYieldModifiers_(
    const std::vector<ActiveEffect_t>& effects,
    int baseYield,
    int count)
{
    std::vector<std::pair<double, ModifierOp>> stack;
    for (const ActiveEffect_t& effect : effects)
    {
        if (!effect.config)
            continue;
        const TileYieldModifierEffect_t* pTileModifier =
            std::get_if<TileYieldModifierEffect_t>(&effect.config->effect);
        if (!pTileModifier)
            continue;

        double amount = pTileModifier->amount;
        // Add-type bonuses scale with matched-tile count; count=0 → zero contribution.
        if (pTileModifier->op == ModifierOp::Add)
            amount *= static_cast<double>(count);
        stack.emplace_back(amount, pTileModifier->op);
    }
    return ApplyModifierStack(static_cast<double>(baseYield), stack);
}

int CalculateResourceWithTileYieldModifiers_(
    const std::vector<ActiveEffect_t>& activeEffects,
    StatId resourceId,
    const TileResources_t& worked,
    const WorkerAssignmentManager& workerAssignments,
    const PopContainer& pops,
    const Tile* pBaseTile,
    const TileEffectsContext& rTileEffects)
{
    const TileResources_t baseTile = ComputeBaseTileResources_(pBaseTile, rTileEffects);

    const int baseTileRaw = GetResourceValue_(baseTile, resourceId);
    const double baseTileModified = ResolveTileYieldModifiers_(
        FilterTileYieldModifiersBySelector_(activeEffects, resourceId, TileSelectorKind::BaseTile),
        baseTileRaw, 1);

    int workedModifiedDelta = 0;
    for (const std::string& improvementId : CollectHasImprovementIds_(activeEffects))
    {
        const int count = CountWorkedTilesWithImprovement_(workerAssignments, pops, improvementId);
        const int raw = ComputeWorkedResourceForImprovement_(workerAssignments, pops, improvementId, resourceId, rTileEffects);
        const double modified = ResolveTileYieldModifiers_(
            FilterTileYieldModifiersBySelectorAndImprovement_(activeEffects, resourceId, improvementId),
            raw, count);
        workedModifiedDelta += static_cast<int>(modified) - raw;
    }

    return GetResourceValue_(worked, resourceId) + static_cast<int>(baseTileModified) + workedModifiedDelta;
}

} // namespace

ResourceManager::ResourceManager(
    const PopulationManager* pPopulation,
    const WorkerAssignmentManager* pWorkerAssignments,
    const EconomyManager* pEconomy,
    const BuildingManager* pBuildings,
    const Tile* pBaseTile,
    const TileEffectsContext* pTileEffects)
    : m_pPopulation(pPopulation)
    , m_pWorkerAssignments(pWorkerAssignments)
    , m_pEconomy(pEconomy)
    , m_pBuildings(pBuildings)
    , m_pBaseTile(pBaseTile)
    , m_pTileEffects(pTileEffects)
{
}

ResourceManager::~ResourceManager()
{
}

int ResourceManager::CalculateResource_(StatId stat, const std::vector<ActiveEffect_t>& activeEffects, const TileResources_t& worked) const
{
    if (!m_pWorkerAssignments || !m_pPopulation || !m_pTileEffects)
    {
        throw std::runtime_error("WorkerAssignmentManager, PopulationManager, or TileEffectsContext not set");
    }
    double base = static_cast<double>(CalculateResourceWithTileYieldModifiers_(
        activeEffects, stat, worked, *m_pWorkerAssignments, m_pPopulation->GetContainer(), m_pBaseTile, *m_pTileEffects));
    base += ResolveStatModifiers(FilterByStatId(activeEffects, stat)).total;
    return static_cast<int>(base);
}

int ResourceManager::GetNutrientProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateResource_(StatId::Nutrients, m_activeEffects, m_pWorkerAssignments->ComputeWorkedResources());
}

int ResourceManager::GetMineralProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateResource_(StatId::Minerals, m_activeEffects, m_pWorkerAssignments->ComputeWorkedResources());
}

int ResourceManager::CalculateEcon_(const std::vector<ActiveEffect_t>& activeEffects, int energy) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForEcon(energy)
         + static_cast<int>(ResolveStatModifiers(FilterByStatId(activeEffects, StatId::Econ)).total);
}

int ResourceManager::CalculateLabs_(const std::vector<ActiveEffect_t>& activeEffects, int energy) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForLabs(energy)
         + static_cast<int>(ResolveStatModifiers(FilterByStatId(activeEffects, StatId::Labs)).total);
}

int ResourceManager::CalculatePsych_(const std::vector<ActiveEffect_t>& activeEffects, int energy) const
{
    if (!m_pEconomy)
        throw std::runtime_error("EconomyManager not set");
    return m_pEconomy->CalculateEnergyForPsych(energy)
         + static_cast<int>(ResolveStatModifiers(FilterByStatId(activeEffects, StatId::Psych)).total);
}

int ResourceManager::GetEconProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateEcon_(m_activeEffects, CalculateResource_(StatId::Energy, m_activeEffects, m_pWorkerAssignments->ComputeWorkedResources()));
}

int ResourceManager::GetLabsProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculateLabs_(m_activeEffects, CalculateResource_(StatId::Energy, m_activeEffects, m_pWorkerAssignments->ComputeWorkedResources()));
}

int ResourceManager::GetPsychProduction() const
{
    if (!m_pWorkerAssignments) return 0;
    return CalculatePsych_(m_activeEffects, CalculateResource_(StatId::Energy, m_activeEffects, m_pWorkerAssignments->ComputeWorkedResources()));
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

void ResourceManager::ProduceNutrients_(const std::vector<ActiveEffect_t>& activeEffects, const TileResources_t& worked)
{
    m_nutrients += CalculateResource_(StatId::Nutrients, activeEffects, worked);
}

void ResourceManager::ProduceMinerals_(const std::vector<ActiveEffect_t>& activeEffects, const TileResources_t& worked)
{
    m_minerals += CalculateResource_(StatId::Minerals, activeEffects, worked);
}

void ResourceManager::AllocateEnergy_(const std::vector<ActiveEffect_t>& activeEffects, const TileResources_t& worked)
{
    const int energy = CalculateResource_(StatId::Energy, activeEffects, worked);
    if (!m_pEconomy)
    {
        m_econ += energy;
        return;
    }
    m_econ  += CalculateEcon_(activeEffects, energy);
    m_labs  += CalculateLabs_(activeEffects, energy);
    m_psych += CalculatePsych_(activeEffects, energy);
}

void ResourceManager::ProduceResourcesInternal_(const std::vector<ActiveEffect_t>& activeEffects)
{
    if (!m_pWorkerAssignments)
    {
        throw std::runtime_error("WorkerAssignmentManager not set");
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources();
    ProduceNutrients_(activeEffects, worked);
    ProduceMinerals_(activeEffects, worked);
    AllocateEnergy_(activeEffects, worked);
}

void ResourceManager::ProduceResources(const std::vector<ActiveEffect_t>& activeEffects)
{
    m_activeEffects = activeEffects;
    ProduceResourcesInternal_(m_activeEffects);
}

} // namespace ac
