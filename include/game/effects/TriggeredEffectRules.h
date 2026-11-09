#pragma once

#include "game/effects/TriggeredEffect.h"

#include <span>
#include <string>

namespace ac
{

class Tile;
class Unit;
class UnitDesign;
struct ImprovementConfig_t;

// True when the improvement carries a non-empty on_visit_effects list.
bool ImprovementHasVisitEffects(const ImprovementConfig_t& rHost);

// True when any improvement on the tile carries a non-empty on_visit_effects list.
bool TileHasVisitEffects(const Tile& rTile);

// True when rUnit has a Hold order at a base its faction owns, and its design's
// on_hold_effects contain an entry whose condition matches that base.
bool UnitHasHoldLink(const Unit& rUnit);

// The place that satisfied the first matching hold entry: a BaseHasBuilding facility, else the
// base. Empty without a hold link.
std::string HoldLinkHostName(const Unit& rUnit);

// True when rUnit's design carries any on_detonate_effects. The capability is the list's
// presence, so a new warhead is config alone.
bool UnitCanDetonate(const Unit& rUnit);

// True when rUnit's design carries any on_found_effects. The capability is the list's presence.
bool UnitCanFoundBase(const Unit& rUnit);

// Signed size change from one ModifyPopulation, as ApplyModifyPopulation_ applies it. Shrinks
// honor minSize; growth is uncapped.
int PredictModifyPopulationDelta(int size, const ModifyPopulationEffect_t& rEffect);

// Final population size after applying every ModifyPopulation in order.
int PredictTriggeredPopulationSize(std::span<const TriggeredEffectConfig_t> rEffects, int size);

// Same for a unit design's filled components (production pop costs).
int PredictUnitProductionPopulationSize(const UnitDesign& rDesign, int size);

} // namespace ac
