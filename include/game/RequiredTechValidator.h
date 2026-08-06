#pragma once

namespace ac
{

struct GameDataContext;

// Walks every loaded config with a requiredTech field (buildings, improvements, unit
// components, unit slots, social policies, pop types, council proposals, probe actions)
// and throws std::runtime_error naming the source id and the offending tech if
// requiredTech is non-empty and not a known id in TechRegistry. Throws if techRegistry or
// any walked source registry/config that LoadGameData always installs is null — never
// silently no-ops. Invoked from LoadGameData after all registries are loaded so a typo'd
// required_tech fails at startup, instead of leaving the entry permanently unavailable
// (ValidateEffectReferences covers effect-list references; this covers the separate
// requiredTech field these config types also carry).
void ValidateRequiredTechReferences(const GameDataContext& rData);

} // namespace ac
