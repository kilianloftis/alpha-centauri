#pragma once

#include <string>
#include <vector>

namespace ac
{

class Unit;
class Tile;

// Whether the attacker could fight on rTargetTile: CanEnterTile (attack ⇒ enter) *and*
// Resolve(attack_tile) for the attacker's domain × footing. The second half is what keeps
// "may assault from a boat" separate from "may move across water" — Amphibious Pods opens
// the attack_tile grid without opening enter.
bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile);

// The candidate with the best DefenderWinChance against rAttacker; ties keep candidate order.
// Null when there are no candidates.
Unit* SelectDefender(const Unit& rAttacker, const std::vector<Unit*>& rCandidates);

// The hostile on rTile that would defend against rObserver, among those it can see, or nullptr.
// Concealed occupants read as absent. Embarked cargo is eligible only on a Base tile.
Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile);

// Full declare-attack gate used by TryAttack and UI: moves remaining, Chebyshev adjacency,
// a visible hostile, CanAttackTile, and Resolve(attack_unit) (unit + defender-tile overrides).
// Returns that hostile, or nullptr if the attack must not be offered / resolved.
Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile);

inline bool CanDeclareAttack(const Unit& rAttacker, const Tile& rTargetTile)
{
    return FindAttackableHostileOnTile(rAttacker, rTargetTile) != nullptr;
}

// Bombard flag and any movement fragments left. The hotkey uses this before a tile is chosen.
bool CanBombard(const Unit& rAttacker);

// CanBombard, and Chebyshev distance in 1..bombard_range. Own tile and tiles past the range
// are illegal. Visibility and enterability are not required.
bool IsWithinBombardRange(const Unit& rAttacker, const Tile& rTargetTile);

// Bombard targets come from every hostile on the tile, seen or not: surface units, then
// embarked cargo when the tile has a base. Only hostiles that pass attack_unit are targets.

// The bombard-capable hostile that answers with a duel, chosen by SelectDefender, or nullptr.
Unit* ArtilleryDuelTarget(const Unit& rAttacker, const Tile& rTargetTile);

// The hostiles one strike hits in tile order. Empty while any hostile there can bombard,
// even one the attacker may not engage.
std::vector<Unit*> BombardStrikeTargets(const Unit& rAttacker, const Tile& rTargetTile);

// Improvement ids a shot may wreck: everything but Base and terrain features, and only on a
// tile with no units at all, friendly or embarked included.
std::vector<std::string> BombardImprovementTargets(const Tile& rTargetTile);

enum class AttackKind_t
{
    Attack,
    Bombard,
};

// One movement point, or every fragment left when the unit has AttackingEndsTurn or bombards.
int AttackMoveFragments(const Unit& rAttacker, AttackKind_t kind);

} // namespace ac
