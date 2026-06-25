#pragma once

namespace ac
{

// Energy allocation percentages (Econ/Labs/Psych) for a faction.
// The three fields should always sum to 100.
struct EnergyAllocation_t
{
    int econPercent = 40;
    int labsPercent = 50;
    int psychPercent = 10;
};

// Manages faction-wide economic configuration.
// The energy allocation split is owned here and used by each base's ResourceManager.
class EconomyManager
{
public:
    EconomyManager();
    ~EconomyManager();

    // Set the faction-wide energy allocation percentages.
    // Caller must ensure the three percentages sum to 100.
    void SetEnergyAllocation(const EnergyAllocation_t& allocation);
    EnergyAllocation_t GetEnergyAllocation() const;

    // Calculate how much of a base's total collected energy goes to each category.
    int CalculateEnergyForEcon(int totalEnergy) const;
    int CalculateEnergyForLabs(int totalEnergy) const;
    int CalculateEnergyForPsych(int totalEnergy) const;

private:
    EnergyAllocation_t m_allocation;
};

} // namespace ac
