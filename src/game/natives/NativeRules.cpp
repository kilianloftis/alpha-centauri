#include "game/natives/NativeRules.h"

#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"

namespace ac
{

bool IsWildNative(const Unit& rUnit)
{
    const FactionSpecies_t species = rUnit.GetFaction().GetDefinition().identity.species;
    return rUnit.GetDesign().IsNativeLife() && IsNativeLifeFaction(species);
}

} // namespace ac
