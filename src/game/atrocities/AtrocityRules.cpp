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

#include <stdexcept>

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
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.difficultyConfig)
    {
        throw std::runtime_error("CommitAtrocity requires a difficultyConfig");
    }
    const std::string& rDifficultyId = rGameState.GetSettings().GetGameRules().difficultyId;
    const DifficultyRules_t& rRules =
        rData.difficultyConfig->RequireForSession(rDifficultyId).rules;
    return rPerpetrator.IsPlayerControlled() ? rRules.playerAtrocityThreshold
                                             : rRules.aiAtrocityThreshold;
}

} // namespace

// AI: Apply SRP
AtrocityOutcome_t ResolveAtrocity(const Faction& rPerpetrator, const Faction* pVictim,
                                  AtrocitySeverityId_t authored)
{
    const GameState& rGameState = CurrentGameState();
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.atrocitiesConfig)
    {
        throw std::runtime_error("CommitAtrocity requires an atrocitiesConfig");
    }
    const AtrocitiesConfig_t& rConfig = *rData.atrocitiesConfig;
    const FactionId_t perpetratorId = rPerpetrator.GetFactionId();
    if (pVictim && pVictim->GetFactionId() == perpetratorId)
    {
        pVictim = nullptr;
    }

    AtrocityOutcome_t outcome;
    std::optional<FactionSpecies_t> victimSpecies;
    if (pVictim)
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
    outcome.severity = authored;
    if (!outcome.bCounted)
    {
        return outcome;
    }

    const int priorSimple = rGameState.GetAtrocityLedger().SimpleCount(perpetratorId);
    const int threshold = AtrocityThreshold_(rGameState, rPerpetrator);
    outcome.severity = EffectiveSeverityId(authored, priorSimple, threshold);
    // A Major act, including a Simple one that just escalated, carries no commerce sanction.
    if (outcome.severity == AtrocitySeverityId_t::Simple)
    {
        outcome.sanctionYears = SanctionYearsAdded(rConfig, priorSimple + 1);
    }
    const AtrocitySeverityConfig_t& rSeverity = rConfig.For(outcome.severity);
    outcome.bUniversalVendetta = rSeverity.bUniversalVendetta;
    outcome.bExpelFromCouncil = rSeverity.bExpelFromCouncil;
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
