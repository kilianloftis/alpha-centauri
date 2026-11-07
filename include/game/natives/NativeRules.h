#pragma once

namespace ac
{

class MoraleCalculator;
class Tile;
class Unit;
class WorldMap;

// Native life owned by a native-life faction, as opposed to a captured or bred native.
bool IsWildNative(const Unit& rUnit);

// Energy credits paid to the faction that kills rVictim: its planet_pearls at its lifecycle level.
// 0 for anything but a wild native.
int PlanetPearlsBounty(const Unit& rVictim);

// Chebyshev-1 neighbours with Fungus (SMAC morale_alien Fungal Tower term).
int AdjacentFungusCount(const Tile& rTile, const WorldMap& rWorldMap);

// Live lifecycle for a wild native from its design's planet block (+ promotion XP).
int WildNativeLifecycleLevel(const Unit& rUnit, const MoraleCalculator& rMorale);

} // namespace ac
