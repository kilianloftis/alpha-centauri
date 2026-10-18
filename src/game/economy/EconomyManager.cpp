#include "game/economy/EconomyManager.h"

#include <stdexcept>
#include <string>

namespace ac
{

EconomyManager::EconomyManager() = default;

EconomyManager::~EconomyManager() = default;

void EconomyManager::AddEnergy(int amount)
{
    m_energy += amount;
}

int EconomyManager::GetEnergy() const
{
    return m_energy;
}

bool EconomyManager::CanAfford(int cost) const
{
    if (cost < 0)
    {
        throw std::invalid_argument("EconomyManager::CanAfford: negative cost "
                                    + std::to_string(cost));
    }
    return m_energy >= cost;
}

void EconomyManager::SpendEnergy(int cost)
{
    if (!CanAfford(cost))
    {
        throw std::runtime_error("EconomyManager::SpendEnergy: cost " + std::to_string(cost)
                                 + " exceeds treasury " + std::to_string(m_energy));
    }
    m_energy -= cost;
}

void EconomyManager::SetEnergyAllocation(const EnergyAllocation_t& allocation)
{
    if (allocation.econPercent + allocation.labsPercent + allocation.psychPercent != 100)
    {
        throw std::runtime_error("Energy allocation percentages must sum to 100");
    }
    m_allocation = allocation;
}

EnergyAllocation_t EconomyManager::GetEnergyAllocation() const
{
    return m_allocation;
}

} // namespace ac
