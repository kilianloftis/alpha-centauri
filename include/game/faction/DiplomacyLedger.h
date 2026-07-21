#pragma once

#include "game/faction/FactionPair.h"
#include "game/faction/base/BaseTypes.h"
#include <map>
#include <string>

namespace ac
{

// Pairwise diplomatic standing between two factions.
// Ordered from least to most aligned (excluding Vendetta, which is open hostility).
enum class DiplomaticStatus
{
    None, // no affiliation (default)
    Truce,
    Friendship,
    Pact,
    Vendetta
};

// Empty string for None; otherwise the status name (Truce, Friendship, Pact, Vendetta).
std::string ToString(DiplomaticStatus status);

// World-scoped tracker for diplomatic state between factions.
class DiplomacyLedger
{
public:
    DiplomacyLedger() = default;
    ~DiplomacyLedger() = default;

    DiplomaticStatus GetStatus(FactionId_t a, FactionId_t b) const;
    void SetStatus(FactionId_t a, FactionId_t b, DiplomaticStatus status);

    bool HasTruce(FactionId_t a, FactionId_t b) const;
    bool HasFriendship(FactionId_t a, FactionId_t b) const;
    bool HasPact(FactionId_t a, FactionId_t b) const;
    bool HasVendetta(FactionId_t a, FactionId_t b) const;

    bool AreKnown(FactionId_t a, FactionId_t b) const;
    void SetKnown(FactionId_t a, FactionId_t b, bool known = true);

    int GetGrievance(FactionId_t holder, FactionId_t against) const;
    void SetGrievance(FactionId_t holder, FactionId_t against, int value);
    void AddGrievance(FactionId_t holder, FactionId_t against, int delta);

    bool HasInfiltration(FactionId_t infiltrator, FactionId_t target) const;
    void SetInfiltration(FactionId_t infiltrator, FactionId_t target, bool infiltrated = true);

    int GetIntegrity(FactionId_t faction) const;
    void SetIntegrity(FactionId_t faction, int value);
    void AddIntegrity(FactionId_t faction, int delta);

private:
    std::map<FactionPair, DiplomaticStatus> m_statuses;
    std::map<FactionPair, bool> m_known;
    std::map<DirectedFactionPair, int> m_grievances;
    std::map<DirectedFactionPair, bool> m_infiltration;
    std::map<FactionId_t, int> m_integrity;
};

} // namespace ac
