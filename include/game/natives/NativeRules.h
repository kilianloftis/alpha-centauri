#pragma once

namespace ac
{

class Unit;

// Native life owned by a native-life faction, as opposed to a captured or bred native.
bool IsWildNative(const Unit& rUnit);

// Energy credits paid to the faction that kills rVictim: its planet_pearls at its lifecycle level.
// 0 for anything but a wild native.
int PlanetPearlsBounty(const Unit& rVictim);

} // namespace ac
