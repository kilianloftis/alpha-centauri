#pragma once

namespace ac
{

struct GameDataContext;

// Walks every loaded config with a requiredTech field (buildings, improvements, unit
// components, unit slots, social policies, pop types) and throws std::runtime_error naming
// the source id and the offending tech if requiredTech is non-empty and not a known id in
// TechRegistry. Invoked from LoadGameData after all registries are loaded so a typo'd
// required_tech fails at startup, instead of leaving the entry permanently unavailable
// (ValidateEffectReferences covers effect-list references; this covers the separate
// requiredTech field every one of these config types also carries).
void ValidateRequiredTechReferences(const GameDataContext& rData);

} // namespace ac
