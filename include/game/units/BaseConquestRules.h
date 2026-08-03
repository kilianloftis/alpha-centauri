#pragma once

namespace ac
{

class Unit;
class BaseManager;
class Tile;
class WorldMap;
enum class FactionSpecies_t;

// Pure conquest predicates. World mutations live in BaseConquestEffects.

// Owner units that still hold the base against capture.
bool HasBaseGarrison(const BaseManager& rBase, const WorldMap& rWorldMap);

// Land units need Amphibious to attack a garrison on a water-tile base.
// Other domains are unrestricted here (sea/air coastal fights stay open).
bool CanAttackIntoBaseTile(const Unit& rAttacker, const Tile& rBaseTile);

// Capturer may take an undefended base once on its tile. Domain / amphibious
// entry is gated by TryStep (CanEnterTile) and the garrison by HasBaseGarrison, so this
// asks only about the unit: CannotCaptureBases is the sole veto (Needlejet / Missile
// chassis, noncombat weapon modules).
bool CanCaptureBase(const Unit& rCapturer);

// Human ↔ Progenitor conquest uses the harsher population rule.
bool IsCrossSpeciesConquest(FactionSpecies_t attacker, FactionSpecies_t defender);

// Faction whose units raid undefended bases instead of capturing them.
bool IsNativeLifeFaction(FactionSpecies_t species);

} // namespace ac
