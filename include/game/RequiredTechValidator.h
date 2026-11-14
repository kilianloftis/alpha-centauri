#pragma once

namespace ac
{

struct GameDataContext;

// Every loaded config's required_techs ids must name a tech in TechRegistry, so a typo fails at
// startup. Throws on any null registry.
void ValidateRequiredTechReferences(const GameDataContext& rData);

} // namespace ac
