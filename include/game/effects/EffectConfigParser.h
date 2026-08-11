#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// Shared JSON parsing for the effect system. Used by every config parser that
// defines EffectConfig_t entries (buildings, unit components, and future sources such
// as social engineering) so the string<->enum mappings live in exactly one place.
// Snake_case id parsing (ParseStatId / ParseRuleFlagId / ParseSocialRatingId) lives next
// to its enums in EffectEnums.h, at ac scope.
namespace EffectConfigParser
{

ModifierOp_t ParseModifierOp(const std::string& rOp);
EffectScope_t ParseEffectScope(const std::string& rScope);
EffectPersistence_t ParseEffectPersistence(const std::string& rPersistence);
StatModifierEffect_t::AmountSource_t ParseAmountSource(const std::string& rSource);

// Reads parameters[key] as either a JSON number or a numeric string. Returns defaultValue if absent.
double ParseNumber(const nlohmann::json& parameters, const std::string& key, double defaultValue);

// Like ParseNumber, but throws if the key is absent (no silent default).
double RequireNumber(const nlohmann::json& parameters, const std::string& key);

// Parses a Condition_t from a condition JSON object ({ "kind": ..., "value": ... }).
// Called by ParseEffectConfig when an effect entry carries a "condition" field.
// AllOf `"values"` are desugared to nested TargetTileHas alternatives.
Condition_t ParseCondition(const nlohmann::json& conditionJson);

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson);

UnitDomain_t ParseUnitDomain(const std::string& rDomain);

// Parses a UnitFilter_t from a unitFilter JSON object
// ({ "kind": "Domain", "domain": "air" } or { "kind": "HasComponent", "component": "..." }).
UnitFilter_t ParseUnitFilter(const nlohmann::json& filterJson);

// Parses a BuildingFilter_t from a buildingFilter JSON object
// ({ "kind": "All" }, { "kind": "BuildingId", "building": "..." },
//  { "kind": "Category", "category": "grow" }).
BuildingFilter_t ParseBuildingFilter(const nlohmann::json& filterJson);

// Parses a FactionFilter_t from a factionFilter JSON object
// ({ "kind": "ActionTarget" } or { "kind": "CouncilMembers" }).
FactionFilter_t ParseFactionFilter(const nlohmann::json& filterJson);

// Parses a single entry of an "effects" JSON array
// (type/scope/persistence/condition/unitFilter/factionFilter/radius/parameters).
EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson);

// Throws if scope can never be resolved for the given source kind (ThisPop off a pop type,
// ThisUnit off a unit component, ThisBase/ProducedAtThisBase off a source that cannot supply
// an origin base). Deliberately leaves intentional legal-but-inert combos alone (e.g.
// faction-lane effects on improvements, pending territory).
void ValidateScopeForSource(EffectScope_t scope, EffectSourceKind_t sourceKind,
                            const std::string& rSourceId);

// Parses the "effects" array of rContainerJson, if present. Returns {} otherwise.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson);

// As above, plus scope-vs-source validation. rSourceId appears in error messages only.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson,
                                         EffectSourceKind_t sourceKind,
                                         const std::string& rSourceId);

} // namespace EffectConfigParser

} // namespace ac
