#include "game/faction/DiplomacyLedger.h"

namespace ac
{

std::string ToString(DiplomaticStatus status)
{
    switch (status)
    {
    case DiplomaticStatus::None:
        return {};
    case DiplomaticStatus::Truce:
        return "Truce";
    case DiplomaticStatus::Friendship:
        return "Friendship";
    case DiplomaticStatus::Pact:
        return "Pact";
    case DiplomaticStatus::Vendetta:
        return "Vendetta";
    }
    return {};
}

DiplomaticStatus DiplomacyLedger::GetStatus(FactionId_t a, FactionId_t b) const
{
    const FactionPair key = FactionPair::Canonical(a, b);
    const auto it = m_statuses.find(key);
    if (it == m_statuses.end())
    {
        return DiplomaticStatus::None;
    }
    return it->second;
}

void DiplomacyLedger::SetStatus(FactionId_t a, FactionId_t b, DiplomaticStatus status)
{
    const FactionPair key = FactionPair::Canonical(a, b);
    if (status == DiplomaticStatus::None)
    {
        m_statuses.erase(key);
        return;
    }
    m_statuses[key] = status;
}

bool DiplomacyLedger::HasTruce(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Truce;
}

bool DiplomacyLedger::HasFriendship(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Friendship;
}

bool DiplomacyLedger::HasPact(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Pact;
}

bool DiplomacyLedger::HasVendetta(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Vendetta;
}

bool DiplomacyLedger::AreKnown(FactionId_t a, FactionId_t b) const
{
    const FactionPair key = FactionPair::Canonical(a, b);
    const auto it = m_known.find(key);
    return it != m_known.end() && it->second;
}

void DiplomacyLedger::SetKnown(FactionId_t a, FactionId_t b, bool known)
{
    const FactionPair key = FactionPair::Canonical(a, b);
    if (!known)
    {
        m_known.erase(key);
        return;
    }
    m_known[key] = true;
}

void DiplomacyLedger::SetKnown(const std::vector<FactionId_t>& rFactionIds)
{
    for (size_t i = 0; i < rFactionIds.size(); ++i)
    {
        for (size_t j = i + 1; j < rFactionIds.size(); ++j)
        {
            SetKnown(rFactionIds[i], rFactionIds[j], true);
        }
    }
}

int DiplomacyLedger::GetGrievance(FactionId_t holder, FactionId_t against) const
{
    const DirectedFactionPair key = DirectedFactionPair::Make(holder, against);
    const auto it = m_grievances.find(key);
    if (it == m_grievances.end())
    {
        return 0;
    }
    return it->second;
}

void DiplomacyLedger::SetGrievance(FactionId_t holder, FactionId_t against, int value)
{
    const DirectedFactionPair key = DirectedFactionPair::Make(holder, against);
    if (value == 0)
    {
        m_grievances.erase(key);
        return;
    }
    m_grievances[key] = value;
}

void DiplomacyLedger::AddGrievance(FactionId_t holder, FactionId_t against, int delta)
{
    SetGrievance(holder, against, GetGrievance(holder, against) + delta);
}

bool DiplomacyLedger::HasInfiltration(FactionId_t infiltrator, FactionId_t target) const
{
    const DirectedFactionPair key = DirectedFactionPair::Make(infiltrator, target);
    const auto it = m_infiltration.find(key);
    return it != m_infiltration.end() && it->second;
}

void DiplomacyLedger::SetInfiltration(FactionId_t infiltrator, FactionId_t target, bool infiltrated)
{
    const DirectedFactionPair key = DirectedFactionPair::Make(infiltrator, target);
    if (!infiltrated)
    {
        m_infiltration.erase(key);
        return;
    }
    m_infiltration[key] = true;
}

int DiplomacyLedger::GetIntegrity(FactionId_t faction) const
{
    const auto it = m_integrity.find(faction);
    if (it == m_integrity.end())
    {
        return 0;
    }
    return it->second;
}

void DiplomacyLedger::SetIntegrity(FactionId_t faction, int value)
{
    if (value == 0)
    {
        m_integrity.erase(faction);
        return;
    }
    m_integrity[faction] = value;
}

void DiplomacyLedger::AddIntegrity(FactionId_t faction, int delta)
{
    SetIntegrity(faction, GetIntegrity(faction) + delta);
}

} // namespace ac
