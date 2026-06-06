#pragma once

#include <vector>

namespace ac
{

class Base;

class Economy
{
public:
    Economy();
    ~Economy();

    int GetEnergyIncome(std::vector<Base*> bases) const;
    int GetCurrentEnergyReserve() const;
    void SetEnergyReserve(int reserve);
    void AddEnergyReserve(int amount);

private:
    int m_energyReserve = 0;
};

} // namespace ac
