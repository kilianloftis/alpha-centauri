#pragma once

#include "game/faction/base/BaseTypes.h"
#include <vector>

namespace ac
{

class BaseManager;

// Energy allocation percentages (Econ/Labs/Psych)
struct EnergyAllocation_t
{
    int econPercent = 40;
    int labsPercent = 50;
    int psychPercent = 10;
};

class Economy
{
public:
    Economy();
    ~Economy();

    int GetEnergyIncome(std::vector<BaseManager*> bases) const;

    // Energy allocation for this turn (set by EnergyAllocation stage)
    void SetEnergyAllocation(const EnergyAllocation_t& allocation);
    EnergyAllocation_t GetEnergyAllocation() const;

    // Total energy produced this turn (set by EnergyAllocation stage)
    void SetTotalEnergyProduced(int amount);
    int GetTotalEnergyProduced() const;

    // Energy allocated to each category (calculated from total and percentages)
    int GetEnergyForEcon() const;
    int GetEnergyForLabs() const;
    int GetEnergyForPsych() const;

    // Current reserves (already allocated to Econ)
    int GetCurrentEnergyReserve() const;
    void SetEnergyReserve(int reserve);
    void AddEnergyReserve(int amount);

private:
    int m_energyReserve = 0;
    int m_totalEnergyProduced = 0;
    EnergyAllocation_t m_allocation;
};

} // namespace ac
