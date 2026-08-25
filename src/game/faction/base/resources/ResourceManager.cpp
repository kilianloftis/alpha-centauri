#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/Faction.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/faction/base/resources/Inefficiency.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/units/Unit.h"
#include "game/units/UnitOrder.h"

#include <stdexcept>
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
    const BaseManager& rBase,
    const SocialRatingRegistry& rSocialRatings,
    const Tile& rBaseTile,
    const TileEffectsContext& rTileEffects,
    const HomeBaseIndex& rHomeUnits)
    : m_rWorkerAssignments(rWorkerAssignments)
    , m_pEconomy(&rEconomy)
    , m_rBase(rBase)
    , m_rSocialRatings(rSocialRatings)
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
        ResolveBaseStat(rBaseEffects, stat, static_cast<double>(workedVal)));
}

int ResourceManager::GetNutrientProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateResource_(StatId_t::Nutrients, ComputeWorked_(rBaseEffects), rBaseEffects);
}

int ResourceManager::GetMineralProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateResource_(StatId_t::Minerals, ComputeWorked_(rBaseEffects), rBaseEffects);
}

int ResourceManager::GetEnergyProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateResource_(StatId_t::Energy, ComputeWorked_(rBaseEffects), rBaseEffects);
}

int ResourceManager::ApplyInefficiency_(int energy) const
{
    // HQ never loses energy to inefficiency (distance 0, and regardless of Efficiency rating).
    const BaseManager* pHq = m_rBase.GetFaction().GetHeadquarters();
    if (pHq == &m_rBase)
    {
        return energy;
    }

    const int distance = pHq
        ? TabletopDiagonalDistance(m_rBaseTile, pHq->GetTile(),
                                   m_rTileEffects.GetWorldMap().GetWidth())
        : k_DefaultInefficiencyHqDistance;
    const int denominator = InefficiencyDenominatorForRating(
        m_rSocialRatings, m_rBase.GetEffectiveSocialRating(SocialRatingId_t::Efficiency));
    return energy - CalculateInefficiencyLoss(energy, distance, denominator);
}

int ResourceManager::AllocatableEnergy_(const BaseEffects_t& rBaseEffects) const
{
    return ApplyInefficiency_(GetEnergyProduction(rBaseEffects));
}

int ResourceManager::CalculateEcon_(int energy, const BaseEffects_t& rBaseEffects) const
{
    // ResolveBaseStat (FilterBaseLevelByStatId): base-level resolution must never pick up
    // selector-carrying (per-tile) modifiers, even on stats where none make sense today.
    const int split = m_pEconomy->CalculateEnergyForEcon(energy);
    return FinalizeResolvedStat(
        ResolveBaseStat(rBaseEffects, StatId_t::Econ, static_cast<double>(split)));
}

int ResourceManager::CalculateLabs_(int energy, const BaseEffects_t& rBaseEffects) const
{
    const int split = m_pEconomy->CalculateEnergyForLabs(energy);
    return FinalizeResolvedStat(
        ResolveBaseStat(rBaseEffects, StatId_t::Labs, static_cast<double>(split)));
}

int ResourceManager::CalculatePsych_(int energy, const BaseEffects_t& rBaseEffects) const
{
    const int split = m_pEconomy->CalculateEnergyForPsych(energy);
    return FinalizeResolvedStat(
        ResolveBaseStat(rBaseEffects, StatId_t::Psych, static_cast<double>(split)));
}

int ResourceManager::GetEconProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateEcon_(AllocatableEnergy_(rBaseEffects), rBaseEffects);
}

int ResourceManager::GetLabsProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculateLabs_(AllocatableEnergy_(rBaseEffects), rBaseEffects);
}

int ResourceManager::GetPsychProduction(const BaseEffects_t& rBaseEffects) const
{
    return CalculatePsych_(AllocatableEnergy_(rBaseEffects), rBaseEffects);
}

int ResourceManager::ConsumeNutrients()
{
    int consumed = m_nutrients;
    m_nutrients = 0;
    return consumed;
}

int ResourceManager::ConsumeMinerals()
{
    int all = m_minerals;
    SpendMinerals(all);
    return all;
}

int ResourceManager::GetMineralBank() const
{
    return m_minerals;
}

void ResourceManager::SpendMinerals(int amount)
{
    if (amount < 0)
    {
        throw std::invalid_argument("SpendMinerals: amount must be non-negative");
    }
    if (amount > m_minerals)
    {
        throw std::runtime_error("SpendMinerals: amount exceeds mineral bank");
    }
    m_minerals -= amount;
}

void ResourceManager::AddResource(StatId_t stat, int amount)
{
    if (amount < 0)
    {
        throw std::invalid_argument("AddResource: amount must be non-negative");
    }
    switch (stat)
    {
    case StatId_t::Nutrients:
        m_nutrients += amount;
        return;
    case StatId_t::Econ:
        m_econ += amount;
        return;
    case StatId_t::Labs:
        m_labs += amount;
        return;
    case StatId_t::Psych:
        m_psych += amount;
        return;
    default:
        // Minerals are deliberately absent: the only caller is stockpile conversion, whose
        // input is the mineral bank, so crediting minerals here would be a feedback loop.
        // Energy is absent because it is not a bank; it goes through AddAllocatedEnergy.
        throw std::invalid_argument(
            "AddResource: stat is not a creditable resource bank (nutrients, econ, labs, psych)");
    }
}

void ResourceManager::AddAllocatedEnergy(int energy)
{
    if (energy < 0)
    {
        throw std::invalid_argument("AddAllocatedEnergy: energy must be non-negative");
    }
    const int allocatable = ApplyInefficiency_(energy);
    m_econ  += m_pEconomy->CalculateEnergyForEcon(allocatable);
    m_labs  += m_pEconomy->CalculateEnergyForLabs(allocatable);
    m_psych += m_pEconomy->CalculateEnergyForPsych(allocatable);
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
    const int energy = ApplyInefficiency_(
        CalculateResource_(StatId_t::Energy, worked, rBaseEffects));

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
