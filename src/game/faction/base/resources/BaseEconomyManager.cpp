#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/ResourceManager.h"
#include <vector>

namespace ac
{

BaseEconomyManager::BaseEconomyManager()
{
}

void BaseEconomyManager::SetEnergyAllocation(const EnergyAllocation_t& allocation)
{
    m_allocation = allocation;
}

EnergyAllocation_t BaseEconomyManager::GetEnergyAllocation() const
{
    return m_allocation;
}

void BaseEconomyManager::SetTotalEnergyCollected(int amount)
{
    m_totalEnergyCollected = amount;
}

int BaseEconomyManager::GetEnergyForEcon() const
{
    return (m_totalEnergyCollected * m_allocation.econPercent) / 100;
}

int BaseEconomyManager::GetEnergyForLabs() const
{
    return (m_totalEnergyCollected * m_allocation.labsPercent) / 100;
}

int BaseEconomyManager::GetEnergyForPsych() const
{
    return (m_totalEnergyCollected * m_allocation.psychPercent) / 100;
}

BaseEconomyManager::~BaseEconomyManager()
{
}

} // namespace ac
