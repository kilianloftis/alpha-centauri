#pragma once

#include "game/effects/EffectConfig.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;
class GameState;

// True when rFilter selects rCandidate as a diplomatic target of rBeneficiary. Never true for
// rCandidate == rBeneficiary. An absent filter falls back to bDefaultCoversAllOthers, which is
// how the two families differ: a continuous effect defaults to "all others" only at WorldGlobal
// scope, while a triggered SetInfiltration has no scope and always defaults to all others.
// actionTarget is required only when the filter is ActionTarget (probe mission subject, etc.).
bool FactionFilterCoversTarget(const std::optional<FactionFilter_t>& rFilter,
                               bool bDefaultCoversAllOthers,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               const GameState& rState,
                               std::optional<FactionId_t> actionTarget = std::nullopt);

// Continuous overload: defaults to all other factions at WorldGlobal scope only.
bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               const GameState& rState,
                               std::optional<FactionId_t> actionTarget = std::nullopt);

// Ledger sticky bits OR Continuous Infiltration effects active on the infiltrator
// (buildings / faction pool / Planetary Governor lane) that cover the target.
bool HasInfiltration(const GameState& rState, FactionId_t infiltrator, FactionId_t target);

} // namespace ac
