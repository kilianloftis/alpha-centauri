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

// Manages faction-wide economic state: the energy treasury and the allocation split
// used by each base's ResourceManager.
class EconomyManager
{
public:
    EconomyManager();
    ~EconomyManager();

    // Faction energy treasury.
    void AddEnergy(int amount);
    int GetEnergy() const;

    // Set the faction-wide energy allocation percentages.
    // Throws if the three percentages do not sum to 100.
    void SetEnergyAllocation(const EnergyAllocation_t& allocation);
    EnergyAllocation_t GetEnergyAllocation() const;

    // Split a base's collected energy into econ / labs / psych.
    // Labs and psych use floored percentage shares; economy receives the remainder
    // (SMAC residual-economy rule). The three results always sum to totalEnergy.
    int CalculateEnergyForEcon(int totalEnergy) const;
    int CalculateEnergyForLabs(int totalEnergy) const;
    int CalculateEnergyForPsych(int totalEnergy) const;

private:
    int m_energy = 0;
    EnergyAllocation_t m_allocation;
};

} // namespace ac
