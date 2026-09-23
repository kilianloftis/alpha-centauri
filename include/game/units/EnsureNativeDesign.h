#pragma once

#include <string_view>

namespace ac
{

class Faction;
class NativeDesign;
struct GameDataContext;

// Assembles a NativeDesign from the native unit registry and registers it with the faction's
// Military, returning the design (or the existing one with the same id).
//
// Empty nativeId returns nullptr. An id that is not in the registry throws: callers validate
// ids at load, so reaching here with a bad one is a programmer error.
const NativeDesign* EnsureNativeDesign(Faction& rFaction, const GameDataContext& rDataContext,
                                       std::string_view nativeId);

} // namespace ac
