#pragma once

#include "game/faction/DiplomaticStatus.h"
#include "game/faction/base/BaseTypes.h"

namespace ac
{

class GameState;

// The single path for changing the status between a and b. Moves each side's units out of
// whatever the new status's rules no longer allow: the other's territory when
// enter_territory is lost, and shared tiles and the other's bases when share_tiles is lost.
// Vendetta also grants mutual known-contact. Throws when either faction is not in the session.
void ApplyStatusChange(GameState& rGameState, FactionId_t a, FactionId_t b,
                       DiplomaticStatus_t to);

// Once per game turn: ages every status, then steps down each one held for its
// duration_turns.
void ExpireDiplomaticStatuses(GameState& rGameState);

// declarer formally declares Vendetta on target. No-op when they are already at Vendetta or
// either side is native life. Each side's units withdraw from the other's territory. Every
// faction obliged to defend target (IsObligedToDefend) then answers: the player through a
// PactObligationInteraction_t, AI factions at once.
void DeclareVendetta(GameState& rGameState, FactionId_t declarer, FactionId_t target);

// defender formally enters an existing conflict against aggressor on the defending side:
// Vendetta, obliging nobody. No-op when they are already at Vendetta or either side is native
// life. Each side's units withdraw from the other's territory. Used for atrocity universal
// Vendetta (the world defending the victim).
void JoinVendetta(GameState& rGameState, FactionId_t defender, FactionId_t aggressor);

// aggressor attacked victim (combat, bombardment, a detected probe action, an atrocity).
// Unless their status already permits attacks, this is a sneak attack: Vendetta without a
// declaration, so only units sharing tiles or bases move, and the victim's partners are obliged
// as part of the sneak attack. Native life has no diplomacy and is ignored on either side.
void ApplyHostileAct(GameState& rGameState, FactionId_t aggressor, FactionId_t victim);

// partner honors its defensive obligation against aggressor, entering the conflict as the
// config's defensive_obligation_mode says. kind is how the Vendetta that raised the obligation
// began: after a sneak attack the partner withdraws from nothing either.
void HonorDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t aggressor,
                              VendettaKind_t kind);

// partner declines its defensive obligation toward ally: their status steps down one rung.
// Throws when that status carries no obligation.
void DeclineDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t ally);

} // namespace ac
