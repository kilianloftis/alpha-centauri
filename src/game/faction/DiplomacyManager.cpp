#include "game/faction/DiplomacyManager.h"

#include <algorithm>
#include <stdexcept>

namespace ac
{

DiplomacyManager::PairKey_t DiplomacyManager::CanonicalKey_(FactionId_t a, FactionId_t b)
{
    if (a == b)
    {
        throw std::invalid_argument("DiplomacyManager: cannot set or query a faction's status with itself");
    }
    return {std::min(a, b), std::max(a, b)};
}

DiplomaticStatus DiplomacyManager::GetStatus(FactionId_t a, FactionId_t b) const
{
    const PairKey_t key = CanonicalKey_(a, b);
    const auto it = m_statuses.find(key);
    if (it == m_statuses.end())
    {
        return DiplomaticStatus::None;
    }
    return it->second;
}

void DiplomacyManager::SetStatus(FactionId_t a, FactionId_t b, DiplomaticStatus status)
{
    const PairKey_t key = CanonicalKey_(a, b);
    if (status == DiplomaticStatus::None)
    {
        m_statuses.erase(key);
        return;
    }
    m_statuses[key] = status;
}

bool DiplomacyManager::HasTruce(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Truce;
}

bool DiplomacyManager::HasFriendship(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Friendship;
}

bool DiplomacyManager::HasPact(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Pact;
}

bool DiplomacyManager::HasVendetta(FactionId_t a, FactionId_t b) const
{
    return GetStatus(a, b) == DiplomaticStatus::Vendetta;
}

} // namespace ac
