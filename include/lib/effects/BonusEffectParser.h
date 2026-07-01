#pragma once

#include "lib/effects/BonusEffect.h"
#include "lib/effects/EffectEnums.h"
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// Shared JSON parsing for the bonus/effect system. Used by every config parser that
// defines EffectConfig_t entries (buildings, unit components, and future sources such
// as social engineering) so the string<->enum mappings live in exactly one place.
namespace BonusEffectParser
{

StatId ParseStatId(const std::string& rStat);
RuleFlagId ParseRuleFlagId(const std::string& rFlag);
SocialRatingId ParseSocialRatingId(const std::string& rRating);
ModifierOp ParseModifierOp(const std::string& rOp);
EffectScope_t ParseEffectScope(const std::string& rScope);
EffectPersistence_t ParseEffectPersistence(const std::string& rPersistence);

// Reads parameters[key] as either a JSON number or a numeric string. Returns defaultValue if absent.
double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue);

ConditionKind ParseConditionKind(const std::string& rKind);

// Parses a Condition_t from a condition JSON object ({ "kind": ..., "value": ... }).
// Called by ParseEffectConfig when an effect entry carries a "condition" field.
Condition_t ParseCondition(const nlohmann::json& conditionJson);

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson);

// Parses a single entry of an "effects" JSON array (type/scope/persistence/condition/parameters).
EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson);

// Parses the "effects" array of rContainerJson, if present. Returns {} otherwise.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson);

} // namespace BonusEffectParser

} // namespace ac
