#pragma once

#include "game/effects/InteractionGridsConfig.h"

#include <span>

namespace ac
{

class Unit;
struct ActiveEffect_t;
struct EffectConfig_t;
struct EffectContext_t;

// Which grids these effects carry an InteractionOverride for. Callers cache the result on
// long-lived sources (designs, faction pools) so a resolve can skip effect collection when
// no override could possibly match. Conservative — scope and condition are all
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

// The acting unit's first matching InteractionOverride whose cell differs from stock, else
// the stock grid cell. Overrides that restate stock are skipped (non-default only), so for
// any concrete query only one polarity can fire. The unit is the only override source: rules
// that belong to a *tile* (a port, a landing pad) are plain code at their call site, so this
// function's inputs never vary by caller. pActingUnit may be null when no unit is in play.
InteractionCell_t ResolveInteractionCell(const InteractionGridsConfig_t& rGrids,
                                         const InteractionQuery_t& rQuery,
                                         const Unit* pActingUnit,
                                         const EffectContext_t& rCtx);

} // namespace ac
