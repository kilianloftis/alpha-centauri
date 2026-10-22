#pragma once

#include "game/probes/ProbeActionConfig.h"

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

// A validated non-allied target with its faction and tile resolved once. Holds live references:
// resolve one per call and never keep it across a UI interaction.
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

// A foreign target of the requested kind rProbe's faction can see on rTile: units must be
// visible, a base only explored.
// TODO: no diplomatic gate; Truce, Treaty and Pact partners are targets too.
std::optional<ProbeTarget_t> ResolveProbeTarget(const Unit& rProbe, const Tile& rTile,
                                                ProbeTargetKind_t kind, GameState& rGameState);

// The base whose effects represent this target: itself, or a unit's home base, else any of its
// faction's bases. Null when the faction has none.
const BaseManager* EffectSourceBase(const ProbeTarget_t& rTarget);

} // namespace ac
