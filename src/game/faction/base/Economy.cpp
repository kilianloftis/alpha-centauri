#include "game/faction/base/Economy.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/ResourceManager.h"
#include <vector>

namespace ac
{

Economy::Economy()
{
}

int Economy::GetEnergyIncome(std::vector<BaseManager*> bases) const
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

int Economy::GetCurrentEnergyReserve() const
{
    return m_energyReserve;
}

void Economy::SetEnergyReserve(int reserve)
{
    m_energyReserve = reserve;
}

void Economy::AddEnergyReserve(int amount)
{
    m_energyReserve += amount;
}

void Economy::SetEnergyAllocation(const EnergyAllocation_t& allocation)
{
    m_allocation = allocation;
}

EnergyAllocation_t Economy::GetEnergyAllocation() const
{
    return m_allocation;
}

void Economy::SetTotalEnergyProduced(int amount)
{
    m_totalEnergyProduced = amount;
}

int Economy::GetTotalEnergyProduced() const
{
    return m_totalEnergyProduced;
}

int Economy::GetEnergyForEcon() const
{
    return (m_totalEnergyProduced * m_allocation.econPercent) / 100;
}

int Economy::GetEnergyForLabs() const
{
    return (m_totalEnergyProduced * m_allocation.labsPercent) / 100;
}

int Economy::GetEnergyForPsych() const
{
    return (m_totalEnergyProduced * m_allocation.psychPercent) / 100;
}

Economy::~Economy()
{
}

} // namespace ac
