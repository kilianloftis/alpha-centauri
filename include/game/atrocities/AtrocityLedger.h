#pragma once

#include "game/atrocities/AtrocityConfig.h"
#include "game/faction/base/BaseTypes.h"
#include "lib/Revision.h"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace ac
{

// One committed atrocity. History, not state: a record is never removed, so the victim's
// memory and the eco term stay stable for the rest of the game.
struct AtrocityRecord_t
{
    FactionId_t perpetrator = 0;
    // Absent for a victimless act: a warhead that destroyed nothing of anyone else's, or one
    // that only destroyed the perpetrator's own. Never the perpetrator.
    std::optional<FactionId_t> victim;
    AtrocitySeverityId_t severity;
    int missionYear = 0;
    // Charter state at the moment of commission. With the Charter repealed the act is not
    // counted, which is what keeps that record out of the eco term.
    bool bCharterInForce = false;
    // The authored act passed every gate (Charter, species). Escalation and sanction length
    // read only these records. An excused act is still in the ledger.
    bool bCounted = false;
};

// World-scoped register of committed atrocities and the standing penalties they carry.
// Sibling of DiplomacyLedger: GameState owns one, and AtrocityEffects is what writes to it.
class AtrocityLedger
{
public:
    AtrocityLedger() = default;

    // record.victim must not name the perpetrator; CommitAtrocity drops a self-victim.
    void Record(AtrocityRecord_t record);
    const std::vector<AtrocityRecord_t>& Records() const { return m_records; }

    // How many counted Simple acts this perpetrator has. Escalation compares this against the
    // session level's atrocity threshold; the sanction curve multiplies by it after the act is
    // recorded. Major acts are not included.
    int SimpleCount(FactionId_t perpetrator) const;

    // "Has this perpetrator ever committed an atrocity against this victim", counted or not.
    bool HasVictimized(FactionId_t perpetrator, FactionId_t victim) const;
    // The same question limited to records answered for as Major.
    bool HasCommittedMajorAgainst(FactionId_t perpetrator, FactionId_t victim) const;

    // Eco-damage mineral term: each counted record contributes its severity's
    // ecoVirtualMinerals. An act the Charter no longer forbids is not counted, so it adds
    // nothing. Needs the config because the weight is authored there.
    int EcoVirtualMinerals(FactionId_t perpetrator, const AtrocitiesConfig_t& rConfig) const;

    // Whether the sanction is standing in missionYear. Answered from the stored expiry rather
    // than from the entry existing, so a reader is right whether or not ExpireSanctions has
    // swept yet this turn.
    bool IsSanctioned(FactionId_t faction, int missionYear) const;
    // Mission year the sanction lifts, or nullopt when the faction has no sanction entry.
    std::optional<int> SanctionUntilYear(FactionId_t faction) const;
    // Adds addedYears onto whatever time is still left. A lapsed or absent sanction starts
    // from missionYear. Returns the new expiry year.
    int ExtendSanction(FactionId_t faction, int missionYear, int addedYears);
    // Housekeeping: drops entries IsSanctioned already reports as lifted.
    void ExpireSanctions(int missionYear);

    // Moves on every Record and every sanction change. CommerceManager validates its per-base
    // memo against this the way it does the diplomacy revision.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    Revision m_revision;
    std::vector<AtrocityRecord_t> m_records;
    std::map<FactionId_t, int> m_sanctionUntilYear;
};

} // namespace ac
