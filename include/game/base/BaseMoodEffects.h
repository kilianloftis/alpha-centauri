#pragma once

#include "game/effects/ActiveEffect.h"

#include <vector>

namespace ac
{

class BaseManager;
struct RiotTier_t;

// A base's mood effects (golden age, riot tiers) straddle two lanes: these appenders split the
// same arrays between the base and the faction pool, so nothing counts twice.

// Base-lane mood effects in force at rBase, stamped with rBase as the origin base.
void AppendBaseMoodBaseLaneEffects(const BaseManager& rBase, std::vector<ActiveEffect_t>& rOut);

// Faction-lane mood effects in force at rBase, stamped with rBase as the origin base so
// per-base conditions (OriginBaseIsHomeBase) can narrow them at the receiving unit.
void AppendBaseMoodFactionLaneEffects(const BaseManager& rBase,
                                      std::vector<ActiveEffect_t>& rOut);

// The riot tier in force at rBase, or nullptr when it is not rioting or no tier matches.
const RiotTier_t* ActiveRiotTierFor(const BaseManager& rBase);

} // namespace ac
