#include "game/faction/EconomyManager.h"

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

int EconomyManager::CalculateEnergyForLabs(int totalEnergy) const
{
    // Integer division (floor). Matches SMAC: labs take their exact percentage share.
    return (totalEnergy * m_allocation.labsPercent) / 100;
}

int EconomyManager::CalculateEnergyForPsych(int totalEnergy) const
{
    // Integer division (floor). Matches SMAC: psych takes its exact percentage share.
    return (totalEnergy * m_allocation.psychPercent) / 100;
}

int EconomyManager::CalculateEnergyForEcon(int totalEnergy) const
{
    // Economy is the residual bucket (SMAC): whatever remains after labs and psych
    // receive their floored shares. This conserves energy — the three categories
    // always sum to totalEnergy — and absorbs all rounding leftovers into econ.
    return totalEnergy
         - CalculateEnergyForLabs(totalEnergy)
         - CalculateEnergyForPsych(totalEnergy);
}

} // namespace ac
