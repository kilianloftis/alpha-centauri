#include "game/atrocities/AtrocityLedger.h"

#include <stdexcept>

namespace ac
{

void AtrocityLedger::Record(AtrocityRecord_t record)
{
    if (record.victim && *record.victim == record.perpetrator)
    {
        throw std::invalid_argument("AtrocityLedger: a faction cannot be its own victim");
    }
    m_records.push_back(std::move(record));
    m_revision.Bump();
}

int AtrocityLedger::SimpleCount(FactionId_t perpetrator) const
{
    int count = 0;
    for (const AtrocityRecord_t& rRecord : m_records)
    {
        if (rRecord.perpetrator == perpetrator && rRecord.bCounted
            && rRecord.severity == AtrocitySeverityId_t::Simple)
        {
            ++count;
        }
    }
    return count;
}

bool AtrocityLedger::HasVictimized(FactionId_t perpetrator, FactionId_t victim) const
{
    for (const AtrocityRecord_t& rRecord : m_records)
    {
        if (rRecord.perpetrator == perpetrator && rRecord.victim && *rRecord.victim == victim)
        {
            return true;
        }
    }
    return false;
}

bool AtrocityLedger::HasCommittedMajorAgainst(FactionId_t perpetrator, FactionId_t victim) const
{
    for (const AtrocityRecord_t& rRecord : m_records)
    {
        if (rRecord.perpetrator == perpetrator && rRecord.victim && *rRecord.victim == victim
            && rRecord.severity == AtrocitySeverityId_t::Major)
        {
            return true;
        }
    }
    return false;
}

int AtrocityLedger::EcoVirtualMinerals(FactionId_t perpetrator,
                                      const AtrocitiesConfig_t& rConfig) const
{
    int total = 0;
    for (const AtrocityRecord_t& rRecord : m_records)
    {
        if (rRecord.perpetrator != perpetrator || !rRecord.bCounted)
        {
            continue;
        }
        total += rConfig.For(rRecord.severity).ecoVirtualMinerals;
    }
    return total;
}

bool AtrocityLedger::IsSanctioned(FactionId_t faction, int missionYear) const
{
    const auto it = m_sanctionUntilYear.find(faction);
    return it != m_sanctionUntilYear.end() && it->second > missionYear;
}

std::optional<int> AtrocityLedger::SanctionUntilYear(FactionId_t faction) const
{
    const auto it = m_sanctionUntilYear.find(faction);
    if (it == m_sanctionUntilYear.end())
    {
        return std::nullopt;
    }
    return it->second;
}

int AtrocityLedger::ExtendSanction(FactionId_t faction, int missionYear, int addedYears)
{
    int& rUntil = m_sanctionUntilYear[faction];
    const int base = rUntil > missionYear ? rUntil : missionYear;
    rUntil = base + addedYears;
    m_revision.Bump();
    return rUntil;
}

void AtrocityLedger::ExpireSanctions(int missionYear)
{
    bool bChanged = false;
    for (auto it = m_sanctionUntilYear.begin(); it != m_sanctionUntilYear.end();)
    {
        if (it->second <= missionYear)
        {
            it = m_sanctionUntilYear.erase(it);
            bChanged = true;
            continue;
        }
        ++it;
    }
    if (bChanged)
    {
        m_revision.Bump();
    }
}

} // namespace ac
