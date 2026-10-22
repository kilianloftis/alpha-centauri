#pragma once

#include "game/effects/EffectConfig.h"
#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;

// Whether a triggered effect's rFilter selects candidate, never the beneficiary itself; without
// a filter, every other faction. actionTarget is read only by ActionTarget filters.
bool FactionFilterCoversTarget(const std::optional<FactionFilter_t>& rFilter,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               std::optional<FactionId_t> actionTarget);

// The same for a continuous effect, which without a filter reaches others only at WorldGlobal.
bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               std::optional<FactionId_t> actionTarget);

// Ledger sticky bits OR Continuous Infiltration effects active on the infiltrator
// (buildings / faction pool / Planetary Governor lane) that cover the target.
bool HasInfiltration(FactionId_t infiltrator, FactionId_t target);

} // namespace ac
