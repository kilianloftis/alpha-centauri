#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/EffectConfig.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <variant>

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

TileResources_t CollectSupplyCrawlYield_(const HomeBaseIndex& rHomeUnits,
                                         const TileEffectsContext& rTileEffects,
                                         const BaseEffects_t& rBaseEffects)
{
    TileResources_t total{0, 0, 0};
    for (const Unit* pUnit : rHomeUnits.GetUnits())
    {
        if (!pUnit || !pUnit->IsSupplyCrawling())
        {
            continue;
        }

        const Tile* pTile = pUnit->GetWorkedTile();
        if (!pTile)
        {
            continue;
        }

        const auto& rOrder = std::get<SupplyCrawlOrder_t>(*pUnit->GetOrder());
        const TileResources_t yield =
            rTileEffects.ResolveTileYield(*pTile, /*bIsBaseTile*/false, rBaseEffects).effective;
        switch (rOrder.resource)
        {
            case StatId_t::Nutrients:
                total.nutrients += yield.nutrients;
                break;
            case StatId_t::Minerals:
                total.minerals += yield.minerals;
                break;
            case StatId_t::Energy:
                total.energy += yield.energy;
                break;
            default:
                break;
        }
    }
    return total;
}

} // namespace

ResourceManager::ResourceManager(
    const WorkerAssignmentManager& rWorkerAssignments,
    const EconomyManager& rEconomy,
    const Tile& rBaseTile,
    const TileEffectsContext& rTileEffects,
    const HomeBaseIndex& rHomeUnits)
    : m_rWorkerAssignments(rWorkerAssignments)
    , m_pEconomy(&rEconomy)
    , m_rBaseTile(rBaseTile)
    , m_rTileEffects(rTileEffects)
    , m_rHomeUnits(rHomeUnits)
{
}

void ResourceManager::RebindEconomy(const EconomyManager& rEconomy)
{
    m_pEconomy = &rEconomy;
}

ResourceManager::~ResourceManager()
{
}

// Total worked resources: worker pops + supply crawlers home to this base + base center.
TileResources_t ResourceManager::ComputeWorked_(const BaseEffects_t& rBaseEffects) const
{
    TileResources_t total = m_rWorkerAssignments.ComputeWorkedResources(rBaseEffects);

    const TileResources_t crawl =
        CollectSupplyCrawlYield_(m_rHomeUnits, m_rTileEffects, rBaseEffects);
    total.nutrients += crawl.nutrients;
    total.energy    += crawl.energy;
    total.minerals  += crawl.minerals;

    const TileResources_t baseTile =
        m_rTileEffects.ResolveTileYield(m_rBaseTile, /*bIsBaseTile*/true, rBaseEffects).effective;
    total.nutrients += baseTile.nutrients;
    total.energy    += baseTile.energy;
    total.minerals  += baseTile.minerals;

    return total;
}

int ResourceManager::CalculateResource_(StatId_t stat, const TileResources_t& worked,
                                        const BaseEffects_t& rBaseEffects) const
{
    // Per-tile yield modifiers are already folded into `worked`; only base-level
    // (non-selector) stat modifiers remain. Seed with the worked value so AddPercent
    // scales it (SeedFor is 0 for Additive and would discard percents).
    const int workedVal = GetResourceValue_(worked, stat);
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, stat),
                             static_cast<double>(workedVal))
            .total);
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
    // FilterBaseLevelByStatId, not FilterByStatId: base-level resolution must never pick up
    // selector-carrying (per-tile) modifiers, even on stats where none make sense today.
    const int split = m_pEconomy->CalculateEnergyForEcon(energy);
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, StatId_t::Econ),
                             static_cast<double>(split))
            .total);
}

int ResourceManager::CalculateLabs_(int energy, const BaseEffects_t& rBaseEffects) const
{
    const int split = m_pEconomy->CalculateEnergyForLabs(energy);
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, StatId_t::Labs),
                             static_cast<double>(split))
            .total);
}

int ResourceManager::CalculatePsych_(int energy, const BaseEffects_t& rBaseEffects) const
{
    const int split = m_pEconomy->CalculateEnergyForPsych(energy);
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, StatId_t::Psych),
                             static_cast<double>(split))
            .total);
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
