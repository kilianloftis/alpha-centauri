#pragma once

namespace ac
{

class Faction;
class MoraleCalculator;
class Unit;

// Pays planet_pearls for a destroyed wild native into the killer's energy treasury.
// A faction-owned native pays nothing. The amount is the design's base Add times the
// victim's intrinsic lifecycle level MultiplyGeometric.
void GrantPlanetPearls(const MoraleCalculator& rMorale, Faction& rKiller, const Unit& rVictim);

} // namespace ac
