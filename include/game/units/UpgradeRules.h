#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ac
{

class Faction;
class Unit;
class UnitDesign;

// Non-empty upgrade_class tags on a design's filled components.
std::set<std::string> UpgradeClasses(const UnitDesign& rDesign);

// True when the unit may open the upgrade list (full moves left; not native life).
bool CanOfferUpgrade(const Unit& rUnit);

// Equal upgrade_class sets plus weapon/armor floors (SMAC Console_upgrade without plan matching).
bool IsValidUpgrade(const Faction& rFaction, const UnitDesign& rOld, const UnitDesign& rNew);

// Faction UnitDesigns that are valid upgrades from rOld (excluding rOld itself).
std::vector<const UnitDesign*> UpgradeTargets(const Faction& rFaction, const UnitDesign& rOld);

// Energy to upgrade one unit from rOld to rNew when IsValidUpgrade; empty when invalid.
std::optional<int> QuoteUpgradeEnergy(const Faction& rFaction, const UnitDesign& rOld,
                                      const UnitDesign& rNew);

} // namespace ac
