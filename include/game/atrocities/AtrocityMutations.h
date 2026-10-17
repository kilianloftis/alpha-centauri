#pragma once

#include "game/atrocities/AtrocityConfig.h"

#include <optional>

namespace ac
{

class Faction;
class GameState;

// Why an act was not answered for. Decided once, so the notice does not re-derive it.
enum class AtrocityExcuse_t
{
    None,
    CharterRepealed,
    ProgenitorParty,
};

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

// Record the atrocity against pVictim (null for a victimless act) and, when the authored act
// passes every gate, charge for it: commerce sanctions, universal Vendetta, and Planetary
// Council expulsion. The record is written either way, because the victim's memory is that
// record. A perpetrator named as its own victim is recorded victimless. Only a counted act
// moves a counter, and only a counted Simple act lengthens the commerce sanction.
AtrocityCommitted_t CommitAtrocity(GameState& rGameState, Faction& rPerpetrator,
                                   Faction* pVictim, AtrocitySeverityId_t severity);

} // namespace ac
