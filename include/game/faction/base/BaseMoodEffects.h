#pragma once

#include "game/effects/ActiveEffect.h"

#include <vector>

namespace ac
{

class BaseManager;
struct RiotTier_t;

// The pop_composition mood arrays (golden age, riot tiers) resolved for one base.
//
// A base's mood effects straddle both lanes: the resource clamps and disable_production are
// ThisBase and resolve through BaseEffectsCache, while the morale penalty is FactionUnits and
// must reach units through the faction pool. These two appenders partition the same config
// arrays between those paths, so nothing is counted twice and nothing is dropped.
//
// All three throw when the owning faction's GameDataContext has no popCompositionConfig:
// callers previously each chose their own silent fallback, which is how a rioting base's
// unit-side penalty came to be configured but never delivered.

// Base-lane mood effects in force at rBase, stamped with rBase as the origin base.
void AppendBaseMoodBaseLaneEffects(const BaseManager& rBase, std::vector<ActiveEffect_t>& rOut);

// Faction-lane mood effects in force at rBase, stamped with rBase as the origin base so
// per-base conditions (OriginBaseIsHomeBase) can narrow them at the receiving unit.
void AppendBaseMoodFactionLaneEffects(const BaseManager& rBase,
                                      std::vector<ActiveEffect_t>& rOut);

// The riot tier in force at rBase, or nullptr when it is not rioting or no tier matches.
const RiotTier_t* ActiveRiotTierFor(const BaseManager& rBase);

} // namespace ac
