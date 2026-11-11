#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include <nlohmann/json.hpp>
#include <string>

namespace ac
{

// JSON parsing for continuous effects, shared by every config that declares them.
namespace EffectConfigParser
{

ModifierOp_t ParseModifierOp(const std::string& rOp);
EffectScope_t ParseEffectScope(const std::string& rScope);
StatModifierEffect_t::AmountSourceSpec_t ParseAmountSource(const nlohmann::json& rSource);

// Parses a { "kind", "value" } condition. AllOf `"values"` desugar to TargetTileHas.
Condition_t ParseCondition(const nlohmann::json& conditionJson);

TileSelector_t ParseTileSelector(const nlohmann::json& selectorJson);

UnitDomain_t ParseUnitDomain(const std::string& rDomain);

// Parses { "kind": "All" | "BuildingId" | "Category" }.
BuildingFilter_t ParseBuildingFilter(const nlohmann::json& filterJson);

// Parses { "kind": "ActionTarget" | "CouncilMembers" | "PlayerType" }.
FactionFilter_t ParseFactionFilter(const nlohmann::json& filterJson);

// From ParseEffectConfig's own dispatch table, so the two cannot disagree.
bool IsEffectType(const std::string& rTypeName);

// One entry of an "effects" array. A one-shot type throws, naming the list it belongs in.
EffectConfig_t ParseEffectConfig(const nlohmann::json& effectJson);

// Throws for an effect sourceKind can never resolve: a scope with no anchor, or an
// amount_source another kind feeds. Legal but inert combinations pass.
void ValidateEffectForSource(const EffectConfig_t& rEffect, EffectSourceKind_t sourceKind,
                             const std::string& rSourceId);

// Parses the "effects" array of rContainerJson, if present. Returns {} otherwise.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson);

// As above, plus scope-vs-source validation. rSourceId appears in error messages only.
std::vector<EffectConfig_t> ParseEffects(const nlohmann::json& rContainerJson,
                                         EffectSourceKind_t sourceKind,
                                         const std::string& rSourceId);

} // namespace EffectConfigParser

} // namespace ac
