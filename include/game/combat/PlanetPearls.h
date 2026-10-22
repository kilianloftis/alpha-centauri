#pragma once

namespace ac
{

class Faction;
class MoraleCalculator;
class Unit;

// Pays planet_pearls for a destroyed wild native: the design's base amount scaled by its
// lifecycle level. A faction-owned native pays nothing.
void GrantPlanetPearls(const MoraleCalculator& rMorale, Faction& rKiller, const Unit& rVictim);

} // namespace ac
