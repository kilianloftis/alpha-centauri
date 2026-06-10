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

class BaseEconomyManager
{
public:
    BaseEconomyManager();
    ~BaseEconomyManager();

    // Set the energy allocation percentages for this base
    void SetEnergyAllocation(const EnergyAllocation_t& allocation);
    EnergyAllocation_t GetEnergyAllocation() const;

    // Accept total energy collected by this base, return allocated amounts
    void SetTotalEnergyCollected(int amount);

    // Energy allocated to each category (calculated from total and percentages)
    int GetEnergyForEcon() const;
    int GetEnergyForLabs() const;
    int GetEnergyForPsych() const;

private:
    int m_totalEnergyCollected = 0;
    EnergyAllocation_t m_allocation;
};

} // namespace ac
