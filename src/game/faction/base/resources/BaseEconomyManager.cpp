#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include <vector>

namespace ac
{

BaseEconomyManager::BaseEconomyManager()
{
}

int BaseEconomyManager::GetEnergyIncome(std::vector<BaseManager*> bases) const
{
    int total = 0;
    for (auto& pBase : bases)
    {
        // Get energy production from ResourceManager if available
        if (pBase)
        {
            ResourceManager* pResourceManager = pBase->GetResourceManager();
            if (pResourceManager)
            {
                total += pResourceManager->GetEnergyProduction();
            }
        }
    }
    return total;
}

int BaseEconomyManager::GetCurrentEnergyReserve() const
{
    return m_energyReserve;
}

void BaseEconomyManager::SetEnergyReserve(int reserve)
{
    m_energyReserve = reserve;
}

void BaseEconomyManager::AddEnergyReserve(int amount)
{
    m_energyReserve += amount;
}

void BaseEconomyManager::SetEnergyAllocation(const EnergyAllocation_t& allocation)
{
    m_allocation = allocation;
}

EnergyAllocation_t BaseEconomyManager::GetEnergyAllocation() const
{
    return m_allocation;
}

void BaseEconomyManager::SetTotalEnergyProduced(int amount)
{
    m_totalEnergyProduced = amount;
}

int BaseEconomyManager::GetTotalEnergyProduced() const
{
    return m_totalEnergyProduced;
}

int BaseEconomyManager::GetEnergyForEcon() const
{
    return (m_totalEnergyProduced * m_allocation.econPercent) / 100;
}

int BaseEconomyManager::GetEnergyForLabs() const
{
    return (m_totalEnergyProduced * m_allocation.labsPercent) / 100;
}

int BaseEconomyManager::GetEnergyForPsych() const
{
    return (m_totalEnergyProduced * m_allocation.psychPercent) / 100;
}

BaseEconomyManager::~BaseEconomyManager()
{
}

} // namespace ac
