#pragma once

#include <string_view>

namespace ac
{

class Faction;
class NativeDesign;
class NativeUnitRegistry;

// Assembles a NativeDesign from rNatives and registers it with the faction's Military,
// returning the design (or the existing one with the same id).
//
// Empty nativeId returns nullptr. An id that is not in rNatives throws: callers validate ids at
// load, so reaching here with a bad one is a programmer error.
const NativeDesign* EnsureNativeDesign(Faction& rFaction, const NativeUnitRegistry& rNatives,
                                       std::string_view nativeId);

} // namespace ac
