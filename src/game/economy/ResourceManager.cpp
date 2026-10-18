#include "game/economy/ResourceManager.h"
#include "game/economy/CommerceCache.h"
#include "game/economy/EconomyManager.h"
#include "game/economy/EconomyRules.h"
#include "game/faction/Faction.h"
#include "game/base/BaseManager.h"
#include "game/base/HomeBaseIndex.h"
#include "game/population/PopulationManager.h"
#include "game/economy/Inefficiency.h"
#include "game/economy/WorkerAssignmentManager.h"
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

} // namespace

ResourceManager::ResourceManager(const CommerceCache& rCommerce, const BaseManager& rBase)
    : m_pCommerce(&rCommerce)
    , m_rBase(rBase)
{
}

void ResourceManager::RebindCommerce(const CommerceCache& rCommerce)
{
    m_pCommerce = &rCommerce;
}

ResourceManager::~ResourceManager()
{
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
    const TileResources_t worked = WorkedYield(m_rBase);
    return CalculateResource_(StatId_t::Nutrients, worked, rBaseEffects);
}

int ResourceManager::GetMineralProduction(const BaseEffects_t& rBaseEffects) const
{
    const TileResources_t worked = WorkedYield(m_rBase);
    return CalculateResource_(StatId_t::Minerals, worked, rBaseEffects);
}

int ResourceManager::GetEnergyProduction(const BaseEffects_t& rBaseEffects) const
{
    const TileResources_t worked = WorkedYield(m_rBase);
    return CalculateResource_(StatId_t::Energy, worked, rBaseEffects);
}

int ResourceManager::CommerceEnergy_() const
{
    if (m_pCommerce == nullptr)
    {
        throw std::runtime_error("ResourceManager: commerce manager is null");
    }
    return m_pCommerce->GetCommerceEnergy(m_rBase);
}

const std::vector<CommercePartnerLine_t>& ResourceManager::GetCommercePartners() const
{
    if (m_pCommerce == nullptr)
    {
        throw std::runtime_error("ResourceManager: commerce manager is null");
    }
    return m_pCommerce->ComputeForBase(m_rBase);
}

EnergySplit_t ResourceManager::ProducedSplit_(const BaseEffects_t& rBaseEffects) const
{
    const int energy = GetEnergyProduction(rBaseEffects) + CommerceEnergy_();
    return AllocateBaseEnergy(m_rBase, energy);
}

int ResourceManager::GetEconProduction(const BaseEffects_t& rBaseEffects) const
{
    return ProducedSplit_(rBaseEffects).econ;
}

int ResourceManager::GetLabsProduction(const BaseEffects_t& rBaseEffects) const
{
    return ProducedSplit_(rBaseEffects).labs;
}

int ResourceManager::GetPsychProduction(const BaseEffects_t& rBaseEffects) const
{
    return ProducedSplit_(rBaseEffects).psych;
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
    const int allocatable = EnergyAfterInefficiency(m_rBase, energy);
    const EnergyAllocation_t allocation =
        m_rBase.GetFaction().GetEconomy().GetEnergyAllocation();
    const EnergySplit_t split = SplitEnergy(allocation, allocatable);
    m_econ  += split.econ;
    m_labs  += split.labs;
    m_psych += split.psych;
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

int ResourceManager::GetPsych() const
{
    return m_psych;
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
    const int produced = CalculateResource_(StatId_t::Energy, worked, rBaseEffects);
    const EnergySplit_t split = AllocateBaseEnergy(m_rBase, produced + CommerceEnergy_());
    m_econ  += split.econ;
    m_labs  += split.labs;
    m_psych += split.psych;
}

void ResourceManager::ProduceResources(const BaseEffects_t& rBaseEffects)
{
    // Psych is the one bank nothing drains: composition reads it every recalculation, and a
    // draining read would make composition flap within a single turn. It is reset here instead,
    // so it always means "psych available this turn".
    m_psych = 0;

    const TileResources_t worked = WorkedYield(m_rBase);
    ProduceNutrients_(worked, rBaseEffects);
    ProduceMinerals_(worked, rBaseEffects);
    AllocateEnergy_(worked, rBaseEffects);
}

} // namespace ac
