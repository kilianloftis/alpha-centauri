#pragma once

namespace ac
{

class Unit;
class Tile;
class WorldMap;
class TileEffectsContext;

// Whether the attacker could fight on rTargetTile: enterability for every domain, plus
// Permission(Attack) for land channel crosses (embarked, or water-ness differs).
bool CanAttackTile(const Unit& rAttacker, const Tile& rTargetTile, const WorldMap& rWorldMap);

// Hostile on rTile that rObserver can see, or nullptr. Concealed occupants read as absent.
// Embarked cargo is eligible only on a Base tile; non-embarked hostiles are preferred.
Unit* FindVisibleHostileOnTile(const Unit& rObserver, const Tile& rTile,
                               const WorldMap& rWorldMap,
                               const TileEffectsContext& rTileEffects);

// Full declare-attack gate used by TryAttack and UI: moves remaining, Chebyshev adjacency,
// a visible hostile, and CanAttackTile. Returns that hostile, or nullptr if the attack
// must not be offered / resolved.
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
