#pragma once

#include "game/effects/EffectConfig.h"
#include "game/faction/base/BaseTypes.h"

#include <optional>
#include <vector>

namespace ac
{

class Faction;
class GameState;

// True when rConfig's scope + factionFilter select rCandidate as a diplomatic target of
// rBeneficiary. Never true for rCandidate == rBeneficiary. actionTarget is required only
// when factionFilter is ActionTarget (probe mission subject, etc.).
bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               const GameState& rState,
                               std::optional<FactionId_t> actionTarget = std::nullopt);

// Instantaneous Infiltration: write DiplomacyLedger bits for every covered target.
// No-op when rConfig is not Instantaneous Infiltration.
void ApplyInfiltrationEffect(GameState& rState,
                             const Faction& rBeneficiary,
                             const EffectConfig_t& rConfig,
                             std::optional<FactionId_t> actionTarget = std::nullopt);

// Apply every Instantaneous Infiltration entry in rEffects.
void ApplyInfiltrationEffects(GameState& rState,
                              const Faction& rBeneficiary,
                              const std::vector<EffectConfig_t>& rEffects,
                              std::optional<FactionId_t> actionTarget = std::nullopt);

// Ledger sticky bits OR Continuous Infiltration effects active on the infiltrator
// (buildings / faction pool / Planetary Governor lane) that cover the target.
bool HasInfiltration(const GameState& rState, FactionId_t infiltrator, FactionId_t target);

} // namespace ac
