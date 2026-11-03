#include "game/diplomacy/DiplomacyLedger.h"

namespace ac
{

DiplomaticStatus_t DiplomacyLedger::GetStatus(FactionId_t a, FactionId_t b) const
{
    const FactionPair key = FactionPair::Canonical(a, b);
    const auto it = m_statuses.find(key);
    if (it == m_statuses.end())
    {
        return DiplomaticStatus_t::Neutral;
    }
    return it->second.status;
}

void DiplomacyLedger::SetStatus(FactionId_t a, FactionId_t b, DiplomaticStatus_t status)
{
    const FactionPair key = FactionPair::Canonical(a, b);
    if (status == DiplomaticStatus_t::Neutral)
    {
        if (m_statuses.erase(key) > 0)
        {
            m_statusRevision.Bump();
        }
        return;
    }
    const auto [it, bInserted] = m_statuses.insert({key, StatusEntry_t{status, 0}});
    if (bInserted || it->second.status != status)
    {
        it->second = StatusEntry_t{status, 0};
        m_statusRevision.Bump();
    }
}

int DiplomacyLedger::GetTurnsHeld(FactionId_t a, FactionId_t b) const
{
    const auto it = m_statuses.find(FactionPair::Canonical(a, b));
    return it == m_statuses.end() ? 0 : it->second.turnsHeld;
}

void DiplomacyLedger::AgeStatuses()
{
    for (auto& [rPair, rEntry] : m_statuses)
    {
        ++rEntry.turnsHeld;
    }
}

std::vector<FactionPair> DiplomacyLedger::GetStatusPairs() const
{
    std::vector<FactionPair> pairs;
    pairs.reserve(m_statuses.size());
    for (const auto& [rPair, rEntry] : m_statuses)
    {
        pairs.push_back(rPair);
    }
    return pairs;
}

bool DiplomacyLedger::HasVendetta(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus_t::Vendetta;
}

bool DiplomacyLedger::AreKnown(FactionId_t a, FactionId_t b) const
{
    return m_known.contains(FactionPair::Canonical(a, b));
}

void DiplomacyLedger::SetKnown(FactionId_t a, FactionId_t b, bool known)
{
    const FactionPair key = FactionPair::Canonical(a, b);
    if (known)
    {
        m_known.insert(key);
    }
    else
    {
        m_known.erase(key);
    }
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

int DiplomacyLedger::GetFriction(FactionId_t holder, FactionId_t against) const
{
    const DirectedFactionPair key = DirectedFactionPair::Make(holder, against);
    const auto it = m_friction.find(key);
    if (it == m_friction.end())
    {
        return 0;
    }
    return it->second;
}

void DiplomacyLedger::SetFriction(FactionId_t holder, FactionId_t against, int value)
{
    const DirectedFactionPair key = DirectedFactionPair::Make(holder, against);
    if (value == 0)
    {
        m_friction.erase(key);
        return;
    }
    m_friction[key] = value;
}

int DiplomacyLedger::GetGifts(FactionId_t giver, FactionId_t recipient) const
{
    const auto it = m_gifts.find(DirectedFactionPair::Make(giver, recipient));
    if (it == m_gifts.end())
    {
        return 0;
    }
    return it->second;
}

void DiplomacyLedger::SetGifts(FactionId_t giver, FactionId_t recipient, int value)
{
    const DirectedFactionPair key = DirectedFactionPair::Make(giver, recipient);
    if (value == 0)
    {
        m_gifts.erase(key);
        return;
    }
    m_gifts[key] = value;
}

bool DiplomacyLedger::HasInfiltration(FactionId_t infiltrator, FactionId_t target) const
{
    return m_infiltration.contains(DirectedFactionPair::Make(infiltrator, target));
}

void DiplomacyLedger::SetInfiltration(FactionId_t infiltrator, FactionId_t target, bool infiltrated)
{
    const DirectedFactionPair key = DirectedFactionPair::Make(infiltrator, target);
    if (infiltrated)
    {
        m_infiltration.insert(key);
    }
    else
    {
        m_infiltration.erase(key);
    }
}

int DiplomacyLedger::GetBlemishes(FactionId_t faction) const
{
    const auto it = m_blemishes.find(faction);
    if (it == m_blemishes.end())
    {
        return 0;
    }
    return it->second;
}

void DiplomacyLedger::SetBlemishes(FactionId_t faction, int value)
{
    if (value == 0)
    {
        m_blemishes.erase(faction);
        return;
    }
    m_blemishes[faction] = value;
}

} // namespace ac
