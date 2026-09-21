#pragma once

#include <span>
#include <string>
#include <string_view>

namespace ac
{

class Faction;
class UnitDesign;
struct GameDataContext;

// Assembles a UnitDesign from a bare list of component ids and registers it with the faction's
// Military, returning the design (or the existing one, since UnitDesign::GetId is derived from
// the components — building the same list twice yields the same design).
//
// This is how config declares a unit without going through the player's unit designer: escape
// colony pods and triggered GrantUnit effects both name components rather than a design id,
// because designs are per-faction and player-authored and there is no registry of named ones.
//
// Empty componentIds returns nullptr — a legitimate "this ruleset has no such unit". An id that
// is not in the component registry throws: callers validate their ids at load (see
// GameDataContext), so reaching here with a bad one is a programmer error, and returning null
// would silently swallow the unit at the one moment the rule fires.
//
// slotPrefix names the generated slots ("escape_slot_0", …); it only has to be unique within
// one design.
const UnitDesign* EnsureAdHocDesign(Faction& rFaction, const GameDataContext& rDataContext,
                                    std::span<const std::string> componentIds,
                                    std::string_view slotPrefix);

} // namespace ac
