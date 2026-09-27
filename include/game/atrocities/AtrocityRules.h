#pragma once

#include "game/atrocities/AtrocityConfig.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

// Pure atrocity decisions. World mutations live in AtrocityEffects.
// Each of these has its own reason to change — the escalation rule, the legality gate, and the
// duration curve are three separate game rules that happen to be consulted together.

// The severity actually answered for, given the perpetrator's counted Simple acts so far.
// The counter is treated as already including this act: while that total is still at or under
// simpleThreshold the act stays as authored. Past the threshold it is answered for as Major.
// An act already authored as Major is returned unchanged. A non-positive threshold disables
// escalation. The caller passes only counted Simple records.
AtrocitySeverityId_t EffectiveSeverityId(AtrocitySeverityId_t authored, int priorSimpleCount,
                                         int simpleThreshold);

// Whether the world answers for this commission: Vendetta, expulsion, the simple counter, and
// a Simple act's commerce sanction. The Charter gates every tier. A Progenitor party excuses
// the act. The record is still written when this returns false.
bool PenaltiesApply(bool bCharterInForce, bool bSpeciesExempt);

// True when either party is a Progenitor. An absent victim is not a Progenitor. Native life is
// not a Progenitor, so a human act against native life is answered for.
bool SpeciesExemptionApplies(FactionSpecies_t perpetrator,
                             std::optional<FactionSpecies_t> victimSpecies);

// Years a counted Simple act adds to a standing sanction. simpleCountAfter is the simple
// counter once this act is included, so the first adds the coefficient, the second twice
// that, and so on. Major acts add none; this function is not called for them.
int SanctionYearsAdded(const AtrocitiesConfig_t& rConfig, int simpleCountAfter);

// Who a blast is answered to. Destroying a base outranks destroying units, and within each the
// first owner the blast reached wins. The detonator is never their own victim, so razing only
// your own ground is victimless. Both lists are in the order the blast reached them.
std::optional<FactionId_t> BlastVictim(const std::vector<FactionId_t>& rBaseOwnersDestroyed,
                                       const std::vector<FactionId_t>& rUnitOwnersDestroyed,
                                       FactionId_t detonator);

} // namespace ac
