#include "game/faction/Economy.h"
#include "game/faction/Base.h"
#include <vector>

namespace ac
{

Economy::Economy()
{
}

int Economy::GetEnergyIncome(std::vector<Base*> bases) const
{
    int total = 0;
    for (auto& base : bases)
    {
        total += base->GetEnergyProduction();
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

Economy::~Economy()
{
}

} // namespace ac
