#include "game/faction/EconomyManager.h"

#include <cmath>

namespace ac
{

EconomyManager::EconomyManager() = default;

EconomyManager::~EconomyManager() = default;

void EconomyManager::SetEnergyAllocation(const EnergyAllocation_t& allocation)
{
    m_allocation = allocation;
}

EnergyAllocation_t EconomyManager::GetEnergyAllocation() const
{
    return m_allocation;
}

int EconomyManager::CalculateEnergyForEcon(int totalEnergy) const
{
    return std::round((totalEnergy * m_allocation.econPercent) / 100.0);
}

int EconomyManager::CalculateEnergyForLabs(int totalEnergy) const
{
    return std::round((totalEnergy * m_allocation.labsPercent) / 100.0);
}

int EconomyManager::CalculateEnergyForPsych(int totalEnergy) const
{
    return std::round((totalEnergy * m_allocation.psychPercent) / 100.0);
}

} // namespace ac
