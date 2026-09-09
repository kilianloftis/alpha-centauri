#pragma once

#include "game/effects/InteractionGridsConfig.h"
#include "game/faction/base/BaseTypes.h"

#include <span>

namespace ac
{

class Tile;
class Unit;
class WorldMap;
struct ActiveEffect_t;
struct EffectConfig_t;
struct EffectContext_t;

// Which grids these effects carry an InteractionOverride for. Callers cache the result on
// long-lived sources (designs, faction pools) so a resolve can skip effect collection when
// no override could possibly match. Conservative — scope, unitFilter, and condition are all
// ignored, so a set bit means "maybe" and a clear bit means "definitely not".
InteractionGridMask_t InteractionMaskOf(std::span<const EffectConfig_t> effects);
InteractionGridMask_t InteractionMaskOf(std::span<const ActiveEffect_t> effects);

// Whether rUnit could carry an InteractionOverride for `grid`. A cheap two-word mask test
// standing in front of CollectLiveUnitEffects, which allocates on every call.
bool UnitMayOverride(const Unit& rUnit, InteractionGridId_t grid);

// Axis values for a single grid lookup. actorDomain is the row on every grid; exactly one
// of the remaining fields is the column, selected by `grid`. The others are ignored.
struct InteractionQuery_t
{
    InteractionGridId_t grid = InteractionGridId_t::Enter;
    UnitDomain_t actorDomain = UnitDomain_t::Land;
    InteractionSurface_t surface = InteractionSurface_t::Land;    // Enter
    InteractionFooting_t footing = InteractionFooting_t::Land;    // AttackTile
    UnitDomain_t targetDomain = UnitDomain_t::Land;               // AttackUnit / Zoc
};

// What rUnit is standing on. Purely a property of the unit — no target tile involved.
InteractionFooting_t FootingFor(const Unit& rUnit);

// Resolve order: acting-unit InteractionOverride (first match) → ThisTile overrides on
// pRelevantTile (improvements + radius-0 projectors for tileFaction) → stock grid cell.
// pActingUnit / pRelevantTile / pWorldMap may be null when that layer is not needed.
InteractionCell_t ResolveInteractionCell(const InteractionGridsConfig_t& rGrids,
                                         const InteractionQuery_t& rQuery,
                                         const Unit* pActingUnit,
                                         const EffectContext_t& rCtx,
                                         const Tile* pRelevantTile,
                                         const WorldMap* pWorldMap,
                                         FactionId_t tileFaction);

} // namespace ac
