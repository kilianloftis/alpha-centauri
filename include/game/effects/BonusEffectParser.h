#pragma once

#include "game/effects/BonusEffect.h"
#include "game/effects/EffectEnums.h"
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// Shared JSON parsing for the bonus/effect system. Used by every config parser that
// defines EffectConfig_t entries (buildings, unit components, and future sources such
// as social engineering) so the string<->enum mappings live in exactly one place.
namespace BonusEffectParser
{

StatId_t ParseStatId(const std::string& rStat);
RuleFlagId_t ParseRuleFlagId(const std::string& rFlag);
SocialRatingId_t ParseSocialRatingId(const std::string& rRating);
ModifierOp_t ParseModifierOp(const std::string& rOp);
EffectScope_t ParseEffectScope(const std::string& rScope);
EffectPersistence_t ParseEffectPersistence(const std::string& rPersistence);
StatModifierEffect_t::AmountSource_t ParseAmountSource(const std::string& rSource);

// Reads parameters[key] as either a JSON number or a numeric string. Returns defaultValue if absent.
double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue);

ConditionKind_t ParseConditionKind(const std::string& rKind);

// Parses a Condition_t from a condition JSON object ({ "kind": ..., "value": ... }).
// Called by ParseEffectConfig when an effect entry carries a "condition" field.
Condition_t ParseCondition(const nlohmann::json& conditionJson);

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson);

UnitDomain_t ParseUnitDomain(const std::string& rDomain);

// Parses a UnitFilter_t from a unitFilter JSON object
// ({ "kind": "Domain", "domain": "air" } or { "kind": "HasComponent", "component": "..." }).
UnitFilter_t ParseUnitFilter(const nlohmann::json& filterJson);

// Parses a single entry of an "effects" JSON array
// (type/scope/persistence/condition/unitFilter/radius/parameters).
EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson);

// Throws if scope can never be resolved for the given source kind (ThisPop off a pop type,
// ThisUnit off a unit component). Deliberately minimal: every other combination loads —
// routing is scope-driven, and combinations whose anchor concept doesn't exist yet (e.g.
// faction-lane effects on improvements, pending territory) stay legal-but-inert.
void ValidateScopeForSource(EffectScope_t scope, EffectSourceKind_t sourceKind,
                            const std::string& rSourceId);

// Parses the "effects" array of rContainerJson, if present. Returns {} otherwise.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson);

// As above, plus scope-vs-source validation. rSourceId appears in error messages only.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson,
                                         EffectSourceKind_t sourceKind,
                                         const std::string& rSourceId);

} // namespace BonusEffectParser

} // namespace ac
