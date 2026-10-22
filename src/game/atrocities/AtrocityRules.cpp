#include "game/atrocities/AtrocityRules.h"

#include "game/CurrentGameState.h"
#include "game/DifficultyConfig.h"
#include "game/GameDataContext.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/faction/Faction.h"


namespace ac
{

namespace
{

std::optional<FactionId_t> FirstForeign_(const std::vector<FactionId_t>& rOwners,
                                         FactionId_t detonator)
{
    for (const FactionId_t owner : rOwners)
    {
        if (owner != detonator)
        {
            return owner;
        }
    }
    return std::nullopt;
}

} // namespace

AtrocitySeverityId_t EffectiveSeverityId(AtrocitySeverityId_t authored, int priorSimpleCount,
                                         int simpleThreshold)
{
    if (simpleThreshold <= 0 || authored == AtrocitySeverityId_t::Major)
    {
        return authored;
    }
    if (priorSimpleCount + 1 <= simpleThreshold)
    {
        return authored;
    }
    return AtrocitySeverityId_t::Major;
}

bool PenaltiesApply(bool bCharterInForce, bool bSpeciesExempt)
{
    return bCharterInForce && !bSpeciesExempt;
}

bool SpeciesExemptionApplies(FactionSpecies_t perpetrator,
                             std::optional<FactionSpecies_t> victimSpecies)
{
    if (perpetrator == FactionSpecies_t::Progenitor)
    {
        return true;
    }
    return victimSpecies.has_value() && *victimSpecies == FactionSpecies_t::Progenitor;
}

int SanctionYearsAdded(const AtrocitiesConfig_t& rConfig, int simpleCountAfter)
{
    return rConfig.sanctionYearsPerAtrocity * simpleCountAfter;
}

std::optional<FactionId_t> BlastVictim(const std::vector<FactionId_t>& rBaseOwnersDestroyed,
                                       const std::vector<FactionId_t>& rUnitOwnersDestroyed,
                                       FactionId_t detonator)
{
    if (const std::optional<FactionId_t> owner = FirstForeign_(rBaseOwnersDestroyed, detonator))
    {
        return owner;
    }
    return FirstForeign_(rUnitOwnersDestroyed, detonator);
}

namespace
{

// No council means no vote has ever repealed anything, and proposals.json declares un_charter
// initially_active, so the Charter is in force, as a freshly built council would report.
bool CharterInForce_(const GameState& rGameState)
{
    const PlanetaryCouncil* pCouncil = rGameState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        return true;
    }
    return pCouncil->HasActiveRuleFlag(RuleFlagId_t::AtrocitiesForbidden);
}

// Handicapping the player is what difficulty is for, so the two sides read separate numbers
// off the same level.
int AtrocityThreshold_(const GameState& rGameState, const Faction& rPerpetrator)
{
    const std::string& rDifficultyId = rGameState.GetSettings().GetGameRules().difficultyId;
    const DifficultyRules_t& rRules =
        rGameState.GetGameData().difficultyConfig->RequireForSession(rDifficultyId).rules;
    return rPerpetrator.IsPlayerControlled() ? rRules.playerAtrocityThreshold
                                             : rRules.aiAtrocityThreshold;
}

// Whether the act is answered for at all: who the victim is, the Charter, and the species
// exemption.
AtrocityOutcome_t Answerability_(const GameState& rGameState, const Faction& rPerpetrator,
                                 const Faction* pVictim)
{
    AtrocityOutcome_t outcome;
    std::optional<FactionSpecies_t> victimSpecies;
    if (pVictim && pVictim->GetFactionId() != rPerpetrator.GetFactionId())
    {
        outcome.victimId = pVictim->GetFactionId();
        victimSpecies = pVictim->GetDefinition().identity.species;
    }
    outcome.bCharterInForce = CharterInForce_(rGameState);
    const bool bSpeciesExempt =
        SpeciesExemptionApplies(rPerpetrator.GetDefinition().identity.species, victimSpecies);
    if (!outcome.bCharterInForce)
    {
        outcome.excuse = AtrocityExcuse_t::CharterRepealed;
    }
    else if (bSpeciesExempt)
    {
        outcome.excuse = AtrocityExcuse_t::ProgenitorParty;
    }
    outcome.bCounted = PenaltiesApply(outcome.bCharterInForce, bSpeciesExempt);
    return outcome;
}

// The severity a counted act is answered at, after escalation, and what that severity carries.
void SetPenalties_(const GameState& rGameState, const Faction& rPerpetrator,
                   AtrocitySeverityId_t authored, AtrocityOutcome_t& rOutcome)
{
    const AtrocitiesConfig_t& rConfig = *rGameState.GetGameData().atrocitiesConfig;
    const int priorSimple =
        rGameState.GetAtrocityLedger().SimpleCount(rPerpetrator.GetFactionId());
    const int threshold = AtrocityThreshold_(rGameState, rPerpetrator);
    rOutcome.severity = EffectiveSeverityId(authored, priorSimple, threshold);
    // A Major act, including a Simple one that just escalated, carries no commerce sanction.
    if (rOutcome.severity == AtrocitySeverityId_t::Simple)
    {
        rOutcome.sanctionYears = SanctionYearsAdded(rConfig, priorSimple + 1);
    }
    const AtrocitySeverityConfig_t& rSeverity = rConfig.For(rOutcome.severity);
    rOutcome.bUniversalVendetta = rSeverity.bUniversalVendetta;
    rOutcome.bExpelFromCouncil = rSeverity.bExpelFromCouncil;
}

} // namespace

AtrocityOutcome_t ResolveAtrocity(const Faction& rPerpetrator, const Faction* pVictim,
                                  AtrocitySeverityId_t authored)
{
    const GameState& rGameState = CurrentGameState();
    AtrocityOutcome_t outcome = Answerability_(rGameState, rPerpetrator, pVictim);
    outcome.severity = authored;
    if (outcome.bCounted)
    {
        SetPenalties_(rGameState, rPerpetrator, authored, outcome);
    }
    return outcome;
}

std::vector<FactionId_t> UniversalVendettaJoiners(FactionId_t perpetrator,
                                                  std::optional<FactionId_t> victim)
{
    const GameState& rGameState = CurrentGameState();
    const DiplomacyLedger& rDiplomacy = rGameState.GetDiplomacyLedger();
    std::vector<FactionId_t> joiners;
    for (const Faction& rOther : rGameState.Factions())
    {
        const FactionId_t otherId = rOther.GetFactionId();
        if (otherId == perpetrator || (victim && otherId == *victim)
            || rOther.IsPlayerControlled() || rDiplomacy.HasVendetta(otherId, perpetrator))
        {
            continue;
        }
        joiners.push_back(otherId);
    }
    return joiners;
}

} // namespace ac
