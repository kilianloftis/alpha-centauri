#pragma once

namespace ac
{

class Unit;
class Tile;
class WorldMap;
class TileEffectsContext;
struct InteractionGridsConfig_t;

// Whether the attacker could fight on rTargetTile: CanEnterTile (attack ⇒ enter) *and*
// Resolve(attack_tile) for the attacker's domain × footing. The second half is what keeps
// "may assault from a boat" separate from "may move across water" — Amphibious Pods opens
// the attack_tile grid without opening enter.
bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap,
                   const InteractionGridsConfig_t& rGrids);

// Hostile on rTile that rObserver can see, or nullptr. Concealed occupants read as absent.
// Embarked cargo is eligible only on a Base tile; non-embarked hostiles are preferred.
Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile,
                               const WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects);

// Full declare-attack gate used by TryAttack and UI: moves remaining, Chebyshev adjacency,
// a visible hostile, CanAttackTile, and Resolve(attack_unit) (unit + defender-tile overrides).
// Returns that hostile, or nullptr if the attack must not be offered / resolved.
Unit* FindAttackableHostileOnTile(const Unit& rAttacker, const Tile& rTargetTile,
                                  const WorldMap& rWorldMap,
                                  const TileEffectsContext& rTileEffects);

inline bool CanDeclareAttack(const Unit& rAttacker, const Tile& rTargetTile,
                             const WorldMap& rWorldMap,
                             const TileEffectsContext& rTileEffects)
{
    return FindAttackableHostileOnTile(rAttacker, rTargetTile, rWorldMap, rTileEffects)
        != nullptr;
}

} // namespace ac
