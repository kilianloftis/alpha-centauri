#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ac
{

class Faction;
class UnitComponentRegistry;
class UnitDesign;

// Builds and registers a design from component ids, or returns the faction's existing one.
// Config grants units by their components, since designs have no shared names.
const UnitDesign* EnsureAdHocDesign(Faction& rFaction, const UnitComponentRegistry& rComponents,
                                    std::span<const std::string> componentIds, // empty: nullptr
                                    std::string_view slotPrefix); // unique within one design

} // namespace ac
