#include "game/atrocities/AtrocityMutations.h"

#include "game/faction/Faction.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/atrocities/AtrocityRules.h"
#include "game/council/CouncilVoteExecutor.h"
#include "game/diplomacy/DiplomaticTransitionMutations.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

// Who did what, and what it costs.
// TODO(atrocity): announced to the player even between unmet AI factions; whether that needs
// contact is unrecorded.
void AnnounceToPlayer_(GameState& rGameState, const Faction& rPerpetrator, const Faction* pVictim,
                       const AtrocityCommitted_t& rCommitted)
{
    const std::string actor = rPerpetrator.GetDefinition().identity.name;
    const std::string against =
        pVictim ? " against " + pVictim->GetDefinition().identity.name : std::string();

    std::string body = actor + " has committed a "
                       + std::string(AtrocitySeverityLabel(rCommitted.severity)) + " atrocity"
                       + against + ".";
    switch (rCommitted.excuse)
    {
    case AtrocityExcuse_t::None:
        if (rCommitted.sanctionUntilYear)
        {
            body += " Commerce sanctions are in force until "
                    + std::to_string(*rCommitted.sanctionUntilYear) + ".";
        }
        break;
    case AtrocityExcuse_t::CharterRepealed:
        body += " The Planetary Council no longer forbids it, so no sanctions follow.";
        break;
    case AtrocityExcuse_t::ProgenitorParty:
        body += " Other faction leaders take no interest, so no sanctions follow.";
        break;
    }

    EnqueueForPlayer(rGameState, NoticeInteraction_t{
                                     PauseOnEventId_t::AtrocityCommitted,
                                     "Atrocity",
                                     std::move(body),
                                     std::nullopt,
                                 });
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
    if (!outcome.victimId)
    {
        pVictim = nullptr;
    }
    AtrocityLedger& rLedger = rGameState.GetAtrocityLedger();
    const FactionId_t perpetratorId = rPerpetrator.GetFactionId();

    const AtrocityRecord_t record =
        MakeRecord_(perpetratorId, outcome, rGameState.GetMissionYear());
    rLedger.Record(record);
    if (outcome.victimId)
    {
        ApplyHostileAct(rGameState, HostileAct_t{.aggressor = perpetratorId,
                                                 .victim = *outcome.victimId,
                                                 .bAttributed = true});
    }

    AtrocityCommitted_t result;
    result.severity = outcome.severity;
    result.excuse = outcome.excuse;
    if (outcome.sanctionYears)
    {
        result.sanctionUntilYear =
            rLedger.ExtendSanction(perpetratorId, record.missionYear, *outcome.sanctionYears);
    }
    RallyWorldAgainst_(rGameState, rPerpetrator, outcome);

    AnnounceToPlayer_(rGameState, rPerpetrator, pVictim, result);
    return result;
}

} // namespace ac
