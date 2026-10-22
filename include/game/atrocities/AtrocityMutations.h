#pragma once

#include "game/atrocities/AtrocityConfig.h"
#include "game/atrocities/AtrocityRules.h"

#include <optional>

namespace ac
{

class Faction;
class GameState;

// What one commission did, for the presentation layer to phrase a notice from.
struct AtrocityCommitted_t
{
    // The severity actually answered for, which escalation may have raised above the one the
    // effect authored.
    AtrocitySeverityId_t severity;
    // None when the act was counted. The record is written either way.
    AtrocityExcuse_t excuse = AtrocityExcuse_t::None;
    // Mission year the commerce sanction lifts. Absent when this act added no sanction: a
    // Major act, or one that was not counted.
    std::optional<int> sanctionUntilYear;

    bool PenaltiesApplied() const { return excuse == AtrocityExcuse_t::None; }
};

// Records the act and, when it counts, charges sanctions, universal Vendetta and expulsion.
// The record is written either way; self-victims are recorded victimless.
AtrocityCommitted_t CommitAtrocity(GameState& rGameState, Faction& rPerpetrator,
                                   Faction* pVictim, AtrocitySeverityId_t severity);

} // namespace ac
