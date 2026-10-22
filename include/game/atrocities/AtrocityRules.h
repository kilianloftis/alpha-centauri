#pragma once

#include "game/atrocities/AtrocityConfig.h"
#include "game/faction/FactionConfig.h"
#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;

// Atrocity decisions; AtrocityMutations carries them out. The escalation rule, the legality gate
// and the duration curve are separate game rules that happen to be consulted together.

// Why an act was not answered for. Decided once, so the notice does not re-derive it.
enum class AtrocityExcuse_t
{
    None,
    CharterRepealed,
    ProgenitorParty,
};

struct AtrocityOutcome_t
{
    // A perpetrator named as its own victim is victimless.
    std::optional<FactionId_t> victimId;
    AtrocityExcuse_t excuse = AtrocityExcuse_t::None;
    bool bCharterInForce = true;
    bool bCounted = false;
    // The severity answered for, which escalation may raise above the authored one.
    AtrocitySeverityId_t severity = AtrocitySeverityId_t::Simple;
    // Years a counted Simple act adds to the commerce sanction.
    std::optional<int> sanctionYears;
    bool bUniversalVendetta = false;
    bool bExpelFromCouncil = false;
};

// The Charter is named first because a repeal excuses everyone; a Progenitor party keeps the
// authored tier, since reclassifying it as Major would punish an act the exemption allows.
AtrocityOutcome_t ResolveAtrocity(const Faction& rPerpetrator, const Faction* pVictim,
                                  AtrocitySeverityId_t authored);

// Living AI factions other than the victim not yet at Vendetta with the perpetrator. Humans are
// never forced in.
std::vector<FactionId_t> UniversalVendettaJoiners(FactionId_t perpetrator,
                                                  std::optional<FactionId_t> victim);

// Simple acts past simpleThreshold, counting this one, are answered as Major; a non-positive
// threshold disables escalation.
AtrocitySeverityId_t EffectiveSeverityId(AtrocitySeverityId_t authored, int priorSimpleCount,
                                         int simpleThreshold);

// Whether the world answers for the act: the Charter gates every tier, and a Progenitor party
// excuses it.
bool PenaltiesApply(bool bCharterInForce, bool bSpeciesExempt);

// True when either party is a Progenitor. An absent victim is not a Progenitor. Native life is
// not a Progenitor, so a human act against native life is answered for.
bool SpeciesExemptionApplies(FactionSpecies_t perpetrator,
                             std::optional<FactionSpecies_t> victimSpecies);

// Years a counted Simple act adds: the coefficient times the Simple count including it.
int SanctionYearsAdded(const AtrocitiesConfig_t& rConfig, int simpleCountAfter);

// A destroyed base outranks destroyed units, the first reached wins, and the detonator is never
// its own victim.
std::optional<FactionId_t> BlastVictim(const std::vector<FactionId_t>& rBaseOwnersDestroyed,
                                       const std::vector<FactionId_t>& rUnitOwnersDestroyed,
                                       FactionId_t detonator);

} // namespace ac
