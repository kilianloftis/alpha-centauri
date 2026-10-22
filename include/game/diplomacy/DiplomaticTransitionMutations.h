#pragma once

#include "game/diplomacy/DiplomaticStatus.h"
#include "game/diplomacy/DiplomaticTransitionRules.h"
#include "game/base/BaseTypes.h"

namespace ac
{

class GameState;

// The one path for changing status between a and b; units leave what the new status no longer
// allows. Vendetta also grants contact. Throws for a faction not in the session.
void ApplyStatusChange(GameState& rGameState, FactionId_t a, FactionId_t b,
                       DiplomaticStatus_t to);

// Once per game turn: ages every status, then steps down each one held for its
// duration_turns.
void ExpireDiplomaticStatuses(GameState& rGameState);

// a cancels its agreement with b, stepping their status down one rung. One-sided, so not a
// proposal. Throws when their status has nothing to cancel.
void CancelTreaty(GameState& rGameState, FactionId_t a, FactionId_t b);

// A formal declaration; a no-op at Vendetta already or with native life. Units withdraw, and
// each faction obliged to defend target answers (the player through an interaction).
void DeclareVendetta(GameState& rGameState, FactionId_t declarer, FactionId_t target);

// Joins an existing conflict on the defending side, obliging nobody, as for an atrocity's
// universal Vendetta. A no-op at Vendetta already or with native life.
void JoinVendetta(GameState& rGameState, FactionId_t defender, FactionId_t aggressor);

// A sneak attack unless the status already permits attacks: Vendetta undeclared, only shared
// tiles move, and the victim's partners are obliged. Ignored if unattributed or native.
void ApplyHostileAct(GameState& rGameState, const HostileAct_t& rAct);

// partner honors its obligation as defensive_obligation_mode says. After a sneak attack the
// partner withdraws from nothing either.
void HonorDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t aggressor,
                              VendettaKind_t kind);

// partner declines its defensive obligation toward ally: their status steps down one rung.
// Throws when that status carries no obligation.
void DeclineDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t ally);

} // namespace ac
