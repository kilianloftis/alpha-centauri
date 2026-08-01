#pragma once

#include "game/units/ProbeActionConfig.h"

#include <optional>
#include <variant>

namespace ac
{

class BaseManager;
class Faction;
class GameState;
class Tile;
class Unit;

struct ProbeBaseTarget_t
{
    BaseManager& rBase;
};

struct ProbeUnitTarget_t
{
    Unit& rUnit;
};

using ProbeTargetRef_t = std::variant<ProbeBaseTarget_t, ProbeUnitTarget_t>;

// A validated non-allied probe target with its faction and tile resolved once, so the
// layers below never re-derive either per target kind.
//
// Short-lived by design: this holds references into live world state, so resolve one per
// call and never store it across a UI interaction — the base may change hands and the
// unit may die between opening a menu and picking from it.
struct ProbeTarget_t
{
    ProbeTargetRef_t ref;
    Faction& rFaction;
    const Tile& rTile;
};

// Config declares an action's target as an enum; this bridges the runtime variant to it.
ProbeTargetKind_t KindOf(const ProbeTargetRef_t& rRef);

// The targeted base, or nullptr for unit targets.
BaseManager* AsBase(const ProbeTarget_t& rTarget);

// Foreign target of the requested kind that rProbe's faction can actually see on rTile,
// or empty when there is none. A base tile resolves either way: base actions target the
// base, unit actions target the first eligible unit stacked there (a garrison is
// subvertible). Units must pass IsUnitVisibleTo; bases only need the tile explored, since
// they stay drawn from explored memory. Concealed occupants therefore yield no target,
// which keeps the order a move until bumping into them reveals them.
// TODO: "foreign" is faction identity only — Truce / Friendship / Pact partners resolve as
// targets with no diplomatic gate or consequence.
std::optional<ProbeTarget_t> ResolveProbeTarget(const Unit& rProbe, const Tile& rTile,
                                                ProbeTargetKind_t kind, GameState& rGameState);

// The base whose effect list represents this target for SE / faction-global resolves
// (probe defense, action cost, success scale). Base targets use themselves; unit targets
// use their home base, falling back to any base of the faction — FactionGlobal effects
// are expanded onto every base, so any of them carries the same value. Null when the
// target faction has no bases at all.
const BaseManager* EffectSourceBase(const ProbeTarget_t& rTarget);

} // namespace ac
