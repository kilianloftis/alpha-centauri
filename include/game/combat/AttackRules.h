#pragma once

#include <string>
#include <vector>

namespace ac
{

class Unit;
class Tile;

// Whether the attacker could fight on rTargetTile: CanEnterTile, and the attack_tile grid for its
// footing, so Amphibious Pods opens the assault without opening movement.
bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile);

// The candidate with the best DefenderWinChance against rAttacker; ties keep candidate order.
// Null when there are no candidates.
Unit* SelectDefender(const Unit& rAttacker, const std::vector<Unit*>& rCandidates);

// The hostile on rTile that would defend against rObserver, among those it can see, or nullptr.
// Concealed occupants read as absent. Embarked cargo is eligible only on a Base tile.
Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile);

// The full declare-attack gate: moves, adjacency, a visible hostile, CanAttackTile and the
// attack_unit grid. The hostile, or nullptr.
Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile);

// Whether a step onto rTile joins the stack there instead of attacking it: rMover may share a
// tile with the owner of the hostile FindVisibleHostileOnTile picks (a Pact partner). Attacking
// that unit stays legal; only the player's step is routed as a move.
bool JoinsSharedStack(const Unit& rMover, const Tile& rTile);

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
