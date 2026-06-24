#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"
#include "game/map/Tile.h"

namespace ac
{

ResourceManager::ResourceManager(
    const PopulationManager* pPopulation,
    const WorkerAssignmentManager* pWorkerAssignments,
    const BaseEconomyManager* pEconomy,
    const BuildingManager* pBuildings,
    std::function<const Tile*(int x, int y)> tileLookup)
    : m_pPopulation(pPopulation)
    , m_pWorkerAssignments(pWorkerAssignments)
    , m_pEconomy(pEconomy)
    , m_pBuildings(pBuildings)
    , m_tileLookup(std::move(tileLookup))
{
}

ResourceManager::~ResourceManager()
{
}

int ResourceManager::CalculateNutrients_() const
{
    if (!m_tileLookup || !m_pWorkerAssignments)
    {
        return 0;
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer(), m_tileLookup);
    // TODO: Add nutrient bonuses from buildings
    return worked.nutrients;
}

int ResourceManager::CalculateMinerals_() const
{
    if (!m_tileLookup || !m_pWorkerAssignments)
    {
        return 0;
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer(), m_tileLookup);
    // TODO: Add mineral bonuses from buildings
    // TODO: Remove minerals from unit upkeep
    return worked.minerals;
}

int ResourceManager::CalculateEnergy_() const
{
    if (!m_tileLookup || !m_pWorkerAssignments)
    {
        return 0;
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer(), m_tileLookup);
    // TODO: Add energy bonuses from buildings
    return worked.energy;
}

int ResourceManager::GetNutrientProduction() const
{
    return CalculateNutrients_();
}

int ResourceManager::GetMineralProduction() const
{
    return CalculateMinerals_();
}

int ResourceManager::GetEconProduction() const
{
    if (!m_pEconomy)
    {
        return CalculateEnergy_();
    }
    m_pEconomy->SetTotalEnergyCollected(CalculateEnergy_());
    return m_pEconomy->GetEnergyForEcon();
}

int ResourceManager::GetLabsProduction() const
{
    if (!m_pEconomy)
    {
        return 0;
    }
    m_pEconomy->SetTotalEnergyCollected(CalculateEnergy_());
    return m_pEconomy->GetEnergyForLabs();
}

int ResourceManager::GetPsychProduction() const
{
    if (!m_pEconomy)
    {
        return 0;
    }
    m_pEconomy->SetTotalEnergyCollected(CalculateEnergy_());
    return m_pEconomy->GetEnergyForPsych();
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

void ResourceManager::ProduceNutrients_()
{
    m_nutrients += CalculateNutrients_();
}

void ResourceManager::ProduceMinerals_()
{
    m_minerals += CalculateMinerals_();
}

void ResourceManager::AllocateEnergy_()
{
    const int totalEnergy = CalculateEnergy_();
    if (!m_pEconomy)
    {
        // No economy manager set, allocate all to econ
        m_econ += totalEnergy;
        return;
    }

    m_pEconomy->SetTotalEnergyCollected(totalEnergy);
    m_econ += m_pEconomy->GetEnergyForEcon();
    m_labs += m_pEconomy->GetEnergyForLabs();
    m_psych += m_pEconomy->GetEnergyForPsych();
}

void ResourceManager::ProduceResources_()
{
    ProduceNutrients_();
    ProduceMinerals_();
    AllocateEnergy_();
}

void ResourceManager::CollectResources()
{
    ProduceResources_();
}

} // namespace ac
