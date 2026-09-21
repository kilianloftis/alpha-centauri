#pragma once

#include "game/effects/TriggeredEffect.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ac
{

// JSON parsing for the one-shot effects in a container's trigger-named lists
// (`on_complete_effects`, `on_enter_effects`, `on_visit_effects`, …). Separate from
// EffectConfigParser because the two families are separate types: an entry in the wrong list
// is a parse error naming both lists, where the old `persistence` flag let it parse and then
// silently never fire.
namespace TriggeredEffectParser
{

// True when typeName is a triggered effect type. EffectConfigParser consults this so a
// triggered entry found in a continuous `effects` array can say which list it belongs in.
bool IsTriggeredEffectType(const std::string& rTypeName);

// True when typeName is a continuous effect type, for the mirror-image error.
bool IsContinuousEffectType(const std::string& rTypeName);

// Parses one entry of a triggered list (type / factionFilter / once_per / parameters).
// rListName appears in error messages only.
TriggeredEffectConfig_t ParseTriggeredEffectConfig(const nlohmann::json& effectJson,
                                                   const std::string& rListName);

// Parses rContainerJson[rKey] if present, else {}. rSourceId appears in error messages only.
std::vector<TriggeredEffectConfig_t> ParseTriggeredEffects(const nlohmann::json& rContainerJson,
                                                           const std::string& rKey,
                                                           const std::string& rSourceId);

} // namespace TriggeredEffectParser

} // namespace ac
