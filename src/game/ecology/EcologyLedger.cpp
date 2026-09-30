#include "game/ecology/EcologyLedger.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

void RequirePositive_(int amount, const char* pWhat)
{
    if (amount < 1)
    {
        throw std::invalid_argument(std::string("EcologyLedger: ") + pWhat
                                    + " amount must be positive");
    }
}

} // namespace

void EcologyLedger::RecordFungalBloom(FactionId_t faction)
{
    ++m_tallies[faction].fungalBlooms;
    m_revision.Bump();
}

void EcologyLedger::GrantCleanMinerals(FactionId_t faction, int amount)
{
    RequirePositive_(amount, "clean mineral grant");
    m_tallies[faction].cleanMineralGrants += amount;
    m_revision.Bump();
}

void EcologyLedger::AddVirtualMinerals(FactionId_t faction, int amount)
{
    RequirePositive_(amount, "virtual minerals");
    m_tallies[faction].virtualMinerals += amount;
    m_revision.Bump();
}

int EcologyLedger::FungalBlooms(FactionId_t faction) const
{
    return Find_(faction).fungalBlooms;
}

int EcologyLedger::CleanMineralGrants(FactionId_t faction) const
{
    return Find_(faction).cleanMineralGrants;
}

int EcologyLedger::VirtualMinerals(FactionId_t faction) const
{
    return Find_(faction).virtualMinerals;
}

EcologyLedger::Tallies_t EcologyLedger::Find_(FactionId_t faction) const
{
    const auto it = m_tallies.find(faction);
    return it == m_tallies.end() ? Tallies_t{} : it->second;
}

} // namespace ac
