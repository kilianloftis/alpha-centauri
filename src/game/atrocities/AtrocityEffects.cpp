#include "game/atrocities/AtrocityEffects.h"

#include "game/DifficultyConfig.h"
#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/atrocities/AtrocityRules.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/DiplomacyStatusEffects.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

// Absent council means no vote has ever repealed anything, and config/council/proposals.json
// declares un_charter initially_active — so the Charter is in force, which is also what a
// freshly built council would report.
bool CharterInForce_(const GameState& rGameState)
{
    const PlanetaryCouncil* pCouncil = rGameState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        return true;
    }
    return pCouncil->HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden);
}

int AtrocityThreshold_(const GameState& rGameState, const Faction& rPerpetrator)
{
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.difficultyConfig)
    {
        throw std::runtime_error("CommitAtrocity requires a difficultyConfig");
    }
    const DifficultyRules_t& rRules =
        rData.difficultyConfig
            ->RequireForSession(rPerpetrator.GetSettings().GetGameRules().difficultyId)
            .rules;
    // Handicapping the player is what difficulty is for, so the two sides read separate
    // numbers off the same level.
    return rPerpetrator.IsPlayerControlled() ? rRules.playerAtrocityThreshold
                                             : rRules.aiAtrocityThreshold;
}

// Living AI factions join Vendetta against the perpetrator, excluding the victim and anyone
// already at Vendetta. Humans are never forced in. Joining (JoinVendetta), not declaring: the
// world defends the victim, so nobody is obliged to defend the perpetrator, and its AI Pact
// partners' Pacts end as they join.
void ApplyUniversalVendetta_(GameState& rGameState, FactionId_t perpetratorId,
                             std::optional<FactionId_t> victimId)
{
    DiplomacyLedger& rDiplomacy = rGameState.GetDiplomacyLedger();

    for (Faction& rOther : rGameState.Factions())
    {
        const FactionId_t otherId = rOther.GetFactionId();
        if (otherId == perpetratorId)
        {
            continue;
        }
        if (victimId && otherId == *victimId)
        {
            continue;
        }
        if (rOther.IsPlayerControlled())
        {
            continue;
        }
        if (rDiplomacy.HasVendetta(otherId, perpetratorId))
        {
            continue;
        }
        JoinVendetta(rGameState, otherId, perpetratorId);
    }
}

// Vendetta and expulsion. Commerce sanctions are a Simple-only curve and are applied by the
// caller. The ledger has already recorded the commission.
void ApplyPenalties_(GameState& rGameState, Faction& rPerpetrator,
                     std::optional<FactionId_t> victimId, AtrocitySeverityId_t severity,
                     const AtrocitiesConfig_t& rConfig)
{
    const AtrocitySeverityConfig_t& rSeverityConfig = rConfig.For(severity);
    if (rSeverityConfig.bUniversalVendetta)
    {
        ApplyUniversalVendetta_(rGameState, rPerpetrator.GetFactionId(), victimId);
    }

    if (rSeverityConfig.bExpelFromCouncil)
    {
        if (PlanetaryCouncil* pCouncil = rGameState.GetPlanetaryCouncil())
        {
            pCouncil->Expel(rPerpetrator);
        }
    }
}

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
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.atrocitiesConfig)
    {
        throw std::runtime_error("CommitAtrocity requires an atrocitiesConfig");
    }
    const AtrocitiesConfig_t& rConfig = *rData.atrocitiesConfig;
    AtrocityLedger& rLedger = rGameState.GetAtrocityLedger();
    const FactionId_t perpetratorId = rPerpetrator.GetFactionId();

    // Wronging yourself is not a victim relationship: razing your own ground is recorded
    // victimless rather than as a record every query then has to discount.
    if (pVictim && pVictim->GetFactionId() == perpetratorId)
    {
        pVictim = nullptr;
    }
    const std::optional<FactionId_t> victimId =
        pVictim ? std::optional<FactionId_t>(pVictim->GetFactionId()) : std::nullopt;

    const bool bCharterInForce = CharterInForce_(rGameState);
    const std::optional<FactionSpecies_t> victimSpecies =
        pVictim ? std::optional<FactionSpecies_t>(pVictim->GetDefinition().identity.species)
                : std::nullopt;
    const bool bSpeciesExempt =
        SpeciesExemptionApplies(rPerpetrator.GetDefinition().identity.species, victimSpecies);
    // Gates on the authored tier. The Charter excuses every tier, and a Progenitor party
    // stays the authored tier: reclassifying it as Major would punish an act the exemption
    // allows. The Charter is named first because a repeal excuses everyone.
    AtrocityCommitted_t result;
    if (!bCharterInForce)
    {
        result.excuse = AtrocityExcuse_t::CharterRepealed;
    }
    else if (bSpeciesExempt)
    {
        result.excuse = AtrocityExcuse_t::ProgenitorParty;
    }
    const bool bCounted = PenaltiesApply(bCharterInForce, bSpeciesExempt);

    AtrocityRecord_t record;
    record.perpetrator = perpetratorId;
    record.victim = victimId;
    record.severity = bCounted ? EffectiveSeverityId(severity, rLedger.SimpleCount(perpetratorId),
                                                     AtrocityThreshold_(rGameState, rPerpetrator))
                               : severity;
    record.missionYear = rGameState.GetMissionYear();
    record.bCharterInForce = bCharterInForce;
    record.bCounted = bCounted;

    rLedger.Record(record);
    if (victimId)
    {
        ApplyHostileAct(rGameState, perpetratorId, *victimId);
    }

    result.severity = record.severity;
    if (bCounted)
    {
        // Major, including a Simple act that just escalated, carries no commerce sanction.
        if (record.severity == AtrocitySeverityId_t::Simple)
        {
            result.sanctionUntilYear = rLedger.ExtendSanction(
                perpetratorId, record.missionYear,
                SanctionYearsAdded(rConfig, rLedger.SimpleCount(perpetratorId)));
        }
        ApplyPenalties_(rGameState, rPerpetrator, victimId, record.severity, rConfig);
    }

    AnnounceToPlayer_(rGameState, rPerpetrator, pVictim, result);
    return result;
}

} // namespace ac
