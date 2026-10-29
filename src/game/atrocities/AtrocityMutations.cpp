#include "game/atrocities/AtrocityMutations.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/atrocities/AtrocityRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/diplomacy/DiplomaticStandingMutations.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"

namespace ac
{

namespace
{

// TODO(atrocity): announced to the player even between unmet AI factions; whether that needs
// contact is unrecorded.
void EnqueueAtrocityNotice_(GameState& rGameState, const Faction& rPerpetrator,
                            const AtrocityCommitted_t& rCommitted)
{
    AtrocityNoticeInteraction_t notice;
    notice.perpetratorName = rPerpetrator.GetDefinition().identity.name;
    if (rCommitted.victimId)
    {
        const Faction& rVictim = rGameState.RequireFaction(*rCommitted.victimId);
        notice.victimName = rVictim.GetDefinition().identity.name;
    }
    notice.severity = rCommitted.severity;
    notice.excuse = rCommitted.excuse;
    notice.sanctionUntilYear = rCommitted.sanctionUntilYear;
    EnqueueForPlayer(rGameState, notice);
}

AtrocityRecord_t MakeRecord_(FactionId_t perpetratorId, const AtrocityOutcome_t& rOutcome,
                             int missionYear)
{
    AtrocityRecord_t record;
    record.perpetrator = perpetratorId;
    record.victim = rOutcome.victimId;
    record.severity = rOutcome.severity;
    record.missionYear = missionYear;
    record.bCharterInForce = rOutcome.bCharterInForce;
    record.bCounted = rOutcome.bCounted;
    return record;
}

// Joining, not declaring: the world defends the victim, so nobody is obliged to defend the
// perpetrator, and its AI Pact partners' Pacts end as they join.
void RallyWorldAgainst_(GameState& rGameState, Faction& rPerpetrator,
                        const AtrocityOutcome_t& rOutcome)
{
    const FactionId_t perpetratorId = rPerpetrator.GetFactionId();
    if (rOutcome.bUniversalVendetta)
    {
        for (const FactionId_t joiner : UniversalVendettaJoiners(perpetratorId, rOutcome.victimId))
        {
            JoinVendetta(rGameState, joiner, perpetratorId);
        }
    }
    if (rOutcome.bExpelFromCouncil)
    {
        rGameState.GetCouncilVotes().Expel(rPerpetrator);
    }
}

} // namespace

AtrocityCommitted_t CommitAtrocity(GameState& rGameState, Faction& rPerpetrator,
                                   Faction* pVictim, AtrocitySeverityId_t severity)
{
    const AtrocityOutcome_t outcome = ResolveAtrocity(rPerpetrator, pVictim, severity);
    AtrocityLedger& rLedger = rGameState.GetAtrocityLedger();
    const FactionId_t perpetratorId = rPerpetrator.GetFactionId();
    if (outcome.victimId)
    {
        ApplyAtrocityVendetta(rGameState, perpetratorId, *outcome.victimId);
        ApplyFriction(rGameState, *outcome.victimId, perpetratorId, outcome.friction);
    }
    ApplyBlemishes(rGameState, perpetratorId, outcome.blemishes);
    const AtrocityRecord_t record =
        MakeRecord_(perpetratorId, outcome, rGameState.GetMissionYear());
    rLedger.Record(record);

    AtrocityCommitted_t result;
    result.severity = outcome.severity;
    result.excuse = outcome.excuse;
    result.victimId = outcome.victimId;
    if (outcome.sanctionYears)
    {
        result.sanctionUntilYear =
            rLedger.ExtendSanction(perpetratorId, record.missionYear, *outcome.sanctionYears);
    }
    RallyWorldAgainst_(rGameState, rPerpetrator, outcome);

    EnqueueAtrocityNotice_(rGameState, rPerpetrator, result);
    return result;
}

} // namespace ac
