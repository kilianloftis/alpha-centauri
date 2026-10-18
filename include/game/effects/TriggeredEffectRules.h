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

// Display name of the place that satisfied the first matching hold entry. Empty when
// UnitHasHoldLink is false. A BaseHasBuilding condition names that facility; otherwise
// the base's own name.
std::string HoldLinkHostName(const Unit& rUnit);

// True when rUnit's design carries any on_detonate_effects. The capability is the list's
// presence, so a new warhead is config alone.
bool UnitCanDetonate(const Unit& rUnit);

// Signed size delta for one ModifyPopulation against a base of `size` (no mutation). The one
// place the op is interpreted — ApplyModifyPopulation_ applies exactly this. Shrinks honor
// minSize; growth is uncapped here (only the applying side knows CanGrow, and the completion
// gates below only care about emptying).
int PredictModifyPopulationDelta(int size, const ModifyPopulationEffect_t& rEffect);

// Final population size after applying every ModifyPopulation in order.
int PredictTriggeredPopulationSize(std::span<const TriggeredEffectConfig_t> rEffects, int size);

// Same for a unit design's filled components (production pop costs).
int PredictUnitProductionPopulationSize(const UnitDesign& rDesign, int size);

} // namespace ac
