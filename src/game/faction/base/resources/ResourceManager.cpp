#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"
#include "game/population/pop-types/Pop.h"
#include "lib/effects/BonusEffect.h"
#include <algorithm>
#include <iterator>

namespace ac
{

namespace
{

std::string ImprovementTypeToString_(ImprovementType type)
{
    switch (type)
    {
        case ImprovementType::Farm: return "Farm";
        case ImprovementType::Condenser: return "Condenser";
    }
    return "";
}

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
    TileSelectorKind selectorKind,
    ImprovementType improvementType)
{
    std::vector<ActiveEffect_t> matching;
    std::copy_if(effects.begin(), effects.end(), std::back_inserter(matching),
                 [resourceId, selectorKind, improvementType](const ActiveEffect_t& effect)
                 {
                     if (!effect.config)
                     {
                         return false;
                     }
                     const TileYieldModifierEffect_t* pTileModifier = std::get_if<TileYieldModifierEffect_t>(&effect.config->effect);
                     if (!pTileModifier || pTileModifier->resource != resourceId ||
                         pTileModifier->selector.kind != selectorKind)
                     {
                         return false;
                     }
                     return pTileModifier->selector.improvement &&
                            *pTileModifier->selector.improvement == improvementType;
                 });
    return matching;
}

TileResources_t ComputeBaseTileResources_(const Tile* pBaseTile)
{
    TileResources_t resources = {};
    if (pBaseTile)
    {
        resources.nutrients = pBaseTile->GetNutrientProduction();
        resources.minerals = pBaseTile->GetMineralProduction();
        resources.energy = pBaseTile->GetEnergyProduction();
    }
    return resources;
}

TileResources_t ComputeTileResourcesForPop_(const Tile& tile, const Pop& pop)
{
    TileResources_t raw;
    raw.nutrients = tile.GetNutrientProduction();
    raw.minerals = tile.GetMineralProduction();
    raw.energy = tile.GetEnergyProduction();
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
    StatId resourceId)
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
        total += GetResourceValue_(ComputeTileResourcesForPop_(*pTile, *pPop), resourceId);
    }
    return total;
}

int CalculateResourceWithTileYieldModifiers_(
    const std::vector<ActiveEffect_t>& activeEffects,
    StatId resourceId,
    const WorkerAssignmentManager& workerAssignments,
    const PopContainer& pops,
    const Tile* pBaseTile)
{
    const TileResources_t worked = workerAssignments.ComputeWorkedResources();
    const TileResources_t baseTile = ComputeBaseTileResources_(pBaseTile);

    const int baseTileRaw = GetResourceValue_(baseTile, resourceId);
    const double baseTileModified = ResolveTileYieldModifiers_(
        FilterTileYieldModifiersBySelector_(activeEffects, resourceId, TileSelectorKind::BaseTile),
        resourceId, baseTileRaw, 1);

    int workedModifiedDelta = 0;
    for (const ImprovementType improvement : {ImprovementType::Farm, ImprovementType::Condenser})
    {
        const std::string improvementId = ImprovementTypeToString_(improvement);
        const int count = CountWorkedTilesWithImprovement_(workerAssignments, pops, improvementId);
        const int raw = ComputeWorkedResourceForImprovement_(workerAssignments, pops, improvementId, resourceId);
        const double modified = ResolveTileYieldModifiers_(
            FilterTileYieldModifiersBySelectorAndImprovement_(activeEffects, resourceId, TileSelectorKind::HasImprovement, improvement),
            resourceId, raw, count);
        workedModifiedDelta += static_cast<int>(modified) - raw;
    }

    return GetResourceValue_(worked, resourceId) + static_cast<int>(baseTileModified) + workedModifiedDelta;
}

double ResolveTileYieldModifiers_(
    const std::vector<ActiveEffect_t>& effects,
    StatId resourceId,
    int baseYield,
    int count)
{
    std::vector<ActiveEffect_t> resolvedEffects;

    EffectConfig_t baseEffectConfig;
    baseEffectConfig.effect = StatModifierEffect_t{resourceId, static_cast<double>(baseYield), ModifierOp::Add};
    baseEffectConfig.scope = EffectScope_t::FactionGlobal;
    baseEffectConfig.persistence = EffectPersistence_t::Continuous;

    ActiveEffect_t baseEffect;
    baseEffect.config = &baseEffectConfig;
    baseEffect.sourceId = "base_tile_yield";
    baseEffect.originBase = nullptr;
    resolvedEffects.push_back(baseEffect);

    for (const ActiveEffect_t& effect : effects)
    {
        if (!effect.config)
        {
            continue;
        }
        const TileYieldModifierEffect_t* pTileModifier = std::get_if<TileYieldModifierEffect_t>(&effect.config->effect);
        if (!pTileModifier || pTileModifier->resource != resourceId)
        {
            continue;
        }

        double amount = pTileModifier->amount;
        ModifierOp op = pTileModifier->op;
        if (op == ModifierOp::Add && count > 0)
        {
            amount *= count;
        }

        EffectConfig_t modifierConfig;
        modifierConfig.effect = StatModifierEffect_t{resourceId, amount, op};
        modifierConfig.scope = EffectScope_t::FactionGlobal;
        modifierConfig.persistence = EffectPersistence_t::Continuous;

        ActiveEffect_t modifierEffect;
        modifierEffect.config = &modifierConfig;
        modifierEffect.sourceId = effect.sourceId;
        modifierEffect.originBase = nullptr;
        resolvedEffects.push_back(modifierEffect);
    }

    return ResolveStatModifiers(resolvedEffects).total;
}

} // namespace

ResourceManager::ResourceManager(
    const PopulationManager* pPopulation,
    const WorkerAssignmentManager* pWorkerAssignments,
    const EconomyManager* pEconomy,
    const BuildingManager* pBuildings,
    const Tile* pBaseTile)
    : m_pPopulation(pPopulation)
    , m_pWorkerAssignments(pWorkerAssignments)
    , m_pEconomy(pEconomy)
    , m_pBuildings(pBuildings)
    , m_pBaseTile(pBaseTile)
{
}

ResourceManager::~ResourceManager()
{
}

int ResourceManager::CalculateNutrients_(const std::vector<ActiveEffect_t>& activeEffects) const
{
    if (!m_pWorkerAssignments || !m_pPopulation)
    {
        throw std::runtime_error("WorkerAssignmentManager or PopulationManager not set");
    }
    double base = static_cast<double>(CalculateResourceWithTileYieldModifiers_(
        activeEffects, StatId::Nutrients, *m_pWorkerAssignments, m_pPopulation->GetContainer(), m_pBaseTile));
    const StatBreakdown_t statModifier = ResolveStatModifiers(
        FilterByStatId(activeEffects, StatId::Nutrients));
    base += statModifier.total;

    return static_cast<int>(base);
}

int ResourceManager::CalculateMinerals_(const std::vector<ActiveEffect_t>& activeEffects) const
{
    if (!m_pWorkerAssignments || !m_pPopulation)
    {
        throw std::runtime_error("WorkerAssignmentManager or PopulationManager not set");
    }
    double base = static_cast<double>(CalculateResourceWithTileYieldModifiers_(
        activeEffects, StatId::Minerals, *m_pWorkerAssignments, m_pPopulation->GetContainer(), m_pBaseTile));
    const StatBreakdown_t statModifier = ResolveStatModifiers(
        FilterByStatId(activeEffects, StatId::Minerals));
    base += statModifier.total;

    return static_cast<int>(base);
}

int ResourceManager::CalculateEnergy_(const std::vector<ActiveEffect_t>& activeEffects) const
{
    if (!m_pWorkerAssignments || !m_pPopulation)
    {
        throw std::runtime_error("WorkerAssignmentManager or PopulationManager not set");
    }
    double base = static_cast<double>(CalculateResourceWithTileYieldModifiers_(
        activeEffects, StatId::Energy, *m_pWorkerAssignments, m_pPopulation->GetContainer(), m_pBaseTile));
    const StatBreakdown_t statModifier = ResolveStatModifiers(
        FilterByStatId(activeEffects, StatId::Energy));
    base += statModifier.total;

    return static_cast<int>(base);
}

int ResourceManager::GetNutrientProduction() const
{
    return CalculateNutrients_(m_activeEffects);
}

int ResourceManager::GetMineralProduction() const
{
    return CalculateMinerals_(m_activeEffects);
}

int ResourceManager::GetEconProduction() const
{
    if (!m_pEconomy)
    {
        throw std::runtime_error("EconomyManager not set");
    }
    return m_pEconomy->CalculateEnergyForEcon(CalculateEnergy_(m_activeEffects));
}

int ResourceManager::GetLabsProduction() const
{
    if (!m_pEconomy)
    {
        throw std::runtime_error("EconomyManager not set");
    }
    return m_pEconomy->CalculateEnergyForLabs(CalculateEnergy_(m_activeEffects));
}

int ResourceManager::GetPsychProduction() const
{
    if (!m_pEconomy)
    {
        throw std::runtime_error("EconomyManager not set");
    }
    return m_pEconomy->CalculateEnergyForPsych(CalculateEnergy_(m_activeEffects));
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

void ResourceManager::ProduceNutrients_(const std::vector<ActiveEffect_t>& activeEffects)
{
    m_nutrients += CalculateNutrients_(activeEffects);
}

void ResourceManager::ProduceMinerals_(const std::vector<ActiveEffect_t>& activeEffects)
{
    m_minerals += CalculateMinerals_(activeEffects);
}

void ResourceManager::AllocateEnergy_(const std::vector<ActiveEffect_t>& activeEffects)
{
    const int totalEnergy = CalculateEnergy_(activeEffects);
    if (!m_pEconomy)
    {
        // No economy manager set, allocate all to econ
        m_econ += totalEnergy;
        return;
    }

    m_econ += m_pEconomy->CalculateEnergyForEcon(totalEnergy);
    m_labs += m_pEconomy->CalculateEnergyForLabs(totalEnergy);
    m_psych += m_pEconomy->CalculateEnergyForPsych(totalEnergy);
}

void ResourceManager::ProduceResourcesInternal_(const std::vector<ActiveEffect_t>& activeEffects)
{
    ProduceNutrients_(activeEffects);
    ProduceMinerals_(activeEffects);
    AllocateEnergy_(activeEffects);
}

void ResourceManager::ProduceResources(const std::vector<ActiveEffect_t>& activeEffects)
{
    m_activeEffects = activeEffects;
    ProduceResourcesInternal_(m_activeEffects);
}

} // namespace ac
