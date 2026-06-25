#include "game/faction/base/resources/ResourceManager.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/PopContainer.h"

namespace ac
{

ResourceManager::ResourceManager(
    const PopulationManager* pPopulation,
    const WorkerAssignmentManager* pWorkerAssignments,
    const EconomyManager* pEconomy,
    const BuildingManager* pBuildings)
    : m_pPopulation(pPopulation)
    , m_pWorkerAssignments(pWorkerAssignments)
    , m_pEconomy(pEconomy)
    , m_pBuildings(pBuildings)
{
}

ResourceManager::~ResourceManager()
{
}

int ResourceManager::CalculateNutrients_() const
{
    if (!m_pWorkerAssignments)
    {
        throw std::runtime_error("WorkerAssignmentManager not set");
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer());
    // TODO: Add nutrient bonuses from buildings
    return worked.nutrients;
}

int ResourceManager::CalculateMinerals_() const
{
    if (!m_pWorkerAssignments)
    {
        throw std::runtime_error("WorkerAssignmentManager not set");
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer());
    // TODO: Add mineral bonuses from buildings
    // TODO: Remove minerals from unit upkeep
    return worked.minerals;
}

int ResourceManager::CalculateEnergy_() const
{
    if (!m_pWorkerAssignments)
    {
        throw std::runtime_error("WorkerAssignmentManager not set");
    }
    const TileResources_t worked = m_pWorkerAssignments->ComputeWorkedResources(
        m_pPopulation->GetContainer());
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
        throw std::runtime_error("EconomyManager not set");
    }
    return m_pEconomy->CalculateEnergyForEcon(CalculateEnergy_());
}

int ResourceManager::GetLabsProduction() const
{
    if (!m_pEconomy)
    {
        throw std::runtime_error("EconomyManager not set");
    }
    return m_pEconomy->CalculateEnergyForLabs(CalculateEnergy_());
}

int ResourceManager::GetPsychProduction() const
{
    if (!m_pEconomy)
    {
        throw std::runtime_error("EconomyManager not set");
    }
    return m_pEconomy->CalculateEnergyForPsych(CalculateEnergy_());
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

    m_econ += m_pEconomy->CalculateEnergyForEcon(totalEnergy);
    m_labs += m_pEconomy->CalculateEnergyForLabs(totalEnergy);
    m_psych += m_pEconomy->CalculateEnergyForPsych(totalEnergy);
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
