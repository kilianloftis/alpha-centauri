#pragma once

#include "game/faction/DiplomaticStatus.h"
#include "game/faction/FactionPair.h"
#include "game/faction/base/BaseTypes.h"
#include "lib/Revision.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ac
{

// World-scoped tracker for diplomatic state between factions.
class DiplomacyLedger
{
public:
    DiplomacyLedger() = default;
    ~DiplomacyLedger() = default;

    DiplomaticStatus_t GetStatus(FactionId_t a, FactionId_t b) const;
    // Changing a pair's status restarts its turns-held count; setting the same status does not.
    void SetStatus(FactionId_t a, FactionId_t b, DiplomaticStatus_t status);

    // Turns the pair has held its current non-Neutral status (0 for Neutral).
    int GetTurnsHeld(FactionId_t a, FactionId_t b) const;
    // Once per game turn: every non-Neutral pair has held its status one turn longer.
    void AgeStatuses();
    // Every pair whose status is not Neutral.
    std::vector<FactionPair> GetStatusPairs() const;

    bool HasVendetta(FactionId_t a, FactionId_t b) const;

    bool AreKnown(FactionId_t a, FactionId_t b) const;
    void SetKnown(FactionId_t a, FactionId_t b, bool known = true);
    // Establish mutual known-contact (commlinks) between every pair in rFactionIds.
    void SetKnown(const std::vector<FactionId_t>& rFactionIds);

    int GetGrievance(FactionId_t holder, FactionId_t against) const;
    void SetGrievance(FactionId_t holder, FactionId_t against, int value);
    void AddGrievance(FactionId_t holder, FactionId_t against, int delta);

    bool HasInfiltration(FactionId_t infiltrator, FactionId_t target) const;
    void SetInfiltration(FactionId_t infiltrator, FactionId_t target, bool infiltrated = true);

    // Moves on every status change. Commerce pairing is treaty-gated, so CommerceManager
    // memoizes against this rather than re-deriving pairs on every query.
    uint64_t GetRevision() const { return m_statusRevision.Get(); }

    int GetIntegrity(FactionId_t faction) const;
    void SetIntegrity(FactionId_t faction, int value);
    void AddIntegrity(FactionId_t faction, int delta);

private:
    Revision m_statusRevision;
    struct StatusEntry_t
    {
        DiplomaticStatus_t status = DiplomaticStatus_t::Neutral;
        int turnsHeld = 0;
    };

    std::map<FactionPair, StatusEntry_t> m_statuses;
    std::set<FactionPair> m_known;
    std::map<DirectedFactionPair, int> m_grievances;
    std::set<DirectedFactionPair> m_infiltration;
    std::map<FactionId_t, int> m_integrity;
};

} // namespace ac
