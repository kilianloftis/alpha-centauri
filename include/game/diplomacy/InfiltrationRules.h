#pragma once

#include "game/effects/EffectConfig.h"
#include "game/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;

// Whether rFilter selects rCandidate, never the beneficiary itself. Without a filter,
// bDefaultCoversAllOthers decides. actionTarget is needed only for ActionTarget filters.
bool FactionFilterCoversTarget(const std::optional<FactionFilter_t>& rFilter,
                               bool bDefaultCoversAllOthers,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               std::optional<FactionId_t> actionTarget = std::nullopt);

// Continuous overload: defaults to all other factions at WorldGlobal scope only.
bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               std::optional<FactionId_t> actionTarget = std::nullopt);

// Ledger sticky bits OR Continuous Infiltration effects active on the infiltrator
// (buildings / faction pool / Planetary Governor lane) that cover the target.
bool HasInfiltration(FactionId_t infiltrator, FactionId_t target);

} // namespace ac
