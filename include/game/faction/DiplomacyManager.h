#pragma once

#include "game/faction/base/BaseTypes.h"
#include <map>
#include <utility>

namespace ac
{

// Pairwise diplomatic standing between two factions.
// Ordered from least to most aligned (excluding Vendetta, which is open hostility).
enum class DiplomaticStatus
{
    None,       // no affiliation (default)
    Truce,
    Friendship,
    Pact,
    Vendetta
};

// World-scoped tracker for symmetric faction relationships.
// Missing pairs default to None. Self-pairs are rejected.
class DiplomacyManager
{
public:
    DiplomacyManager() = default;
    ~DiplomacyManager() = default;

    DiplomaticStatus GetStatus(FactionId_t a, FactionId_t b) const;
    void SetStatus(FactionId_t a, FactionId_t b, DiplomaticStatus status);

    bool HasTruce(FactionId_t a, FactionId_t b) const;
    bool HasFriendship(FactionId_t a, FactionId_t b) const;
    bool HasPact(FactionId_t a, FactionId_t b) const;
    bool HasVendetta(FactionId_t a, FactionId_t b) const;

private:
    using PairKey_t = std::pair<FactionId_t, FactionId_t>;

    static PairKey_t CanonicalKey_(FactionId_t a, FactionId_t b);

    std::map<PairKey_t, DiplomaticStatus> m_statuses;
};

} // namespace ac
