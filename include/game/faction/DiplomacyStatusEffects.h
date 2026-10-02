#pragma once

#include "game/faction/DiplomacyConfig.h"
#include "game/faction/DiplomaticStatus.h"
#include "game/faction/base/BaseTypes.h"

namespace ac
{

class GameState;
class Unit;

// The single path for changing the status between a and b. Moves each side's units out of
// whatever the new status's rules no longer allow: the other's territory when
// enter_territory is lost, and shared tiles and the other's bases when share_tiles is lost.
// Vendetta also grants mutual known-contact.
void ApplyStatusChange(GameState& rGameState, FactionId_t a, FactionId_t b,
                       DiplomaticStatus_t to);

// Once per game turn: ages every status, then steps down each one held for its
// duration_turns.
void ExpireDiplomaticStatuses(GameState& rGameState);

// declarer declares Vendetta on target. No-op when they are already at Vendetta. Every
// faction whose status with target carries a defensive obligation, and that is not already at
// Vendetta with declarer, must then decide whether to declare Vendetta on declarer: the player
// is asked through a PactObligationInteraction_t, AI factions decide at once. Native life has
// no diplomacy: its status changes, but nobody is obliged.
void DeclareVendetta(GameState& rGameState, FactionId_t declarer, FactionId_t target);

// defender enters an existing conflict against aggressor on the defending side: Vendetta,
// obliging nobody. No-op when they are already at Vendetta. Used for honoring an obligation
// as a defender and for atrocity universal Vendetta (the world defending the victim).
void JoinVendetta(GameState& rGameState, FactionId_t defender, FactionId_t aggressor);

// partner honors its defensive obligation against aggressor under mode.
void HonorDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t aggressor,
                              DefensiveObligationMode_t mode);

// aggressor attacked victim (combat, bombardment, a detected probe action, an atrocity).
// Unless their status already permits attacks, aggressor declares Vendetta on victim. Native
// life has no diplomacy and is ignored on either side.
void ApplyHostileAct(GameState& rGameState, FactionId_t aggressor, FactionId_t victim);

// The player's answer when rUnit's move order stopped at territoryOwner's border. Breaking
// declares Vendetta on the owner and resumes the order; otherwise the order is cancelled.
void ResolveTerritoryEntry(GameState& rGameState, Unit& rUnit, FactionId_t territoryOwner,
                           bool bBreakAgreement);

// partner's answer to its defensive obligation toward ally. Declaring honors it
// (HonorDefensiveObligation, under the session's mode); declining steps the partner's status
// with ally down one rung.
void ResolveDefensiveObligation(GameState& rGameState, FactionId_t partner, FactionId_t ally,
                                FactionId_t aggressor, bool bDeclareVendetta);

} // namespace ac
