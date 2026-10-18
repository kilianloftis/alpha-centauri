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

// One line the player can act on: who did what, and what it costs.
// TODO(atrocity): this announces every commission to the human, including one between two AI
// factions they have never met. Whether atrocities are common knowledge (the Datalinks report
// them) or gated on contact is an unrecorded rules question.
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

    AtrocityRecord_t record;
    record.perpetrator = perpetratorId;
    record.victim = outcome.victimId;
    record.severity = outcome.severity;
    record.missionYear = rGameState.GetMissionYear();
    record.bCharterInForce = outcome.bCharterInForce;
    record.bCounted = outcome.bCounted;
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
    // Joining, not declaring: the world defends the victim, so nobody is obliged to defend the
    // perpetrator, and its AI Pact partners' Pacts end as they join.
    if (outcome.bUniversalVendetta)
    {
        for (const FactionId_t joiner : UniversalVendettaJoiners(perpetratorId, outcome.victimId))
        {
            JoinVendetta(rGameState, joiner, perpetratorId);
        }
    }
    if (outcome.bExpelFromCouncil)
    {
        if (CouncilVoteExecutor* pVotes = rGameState.GetCouncilVotes())
        {
            pVotes->Expel(rPerpetrator);
        }
    }

    AnnounceToPlayer_(rGameState, rPerpetrator, pVictim, result);
    return result;
}

} // namespace ac
