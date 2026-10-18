#pragma once

#include "game/research/TechConfigParser.h"
#include "game/research/TechCostConfig.h"

#include <vector>

namespace ac
{

class Faction;
class IEffectsProvider;
class ResearchManager;

// What tech cost_formula reads besides the tech itself. pEffectsProvider is the faction's effect
// pool, and may be null for a manager built outside a faction.
TechCostInputs_t TechCostInputs(const ResearchManager& rResearch,
                                const IEffectsProvider* pEffectsProvider,
                                const TechConfig_t& rTech);

// Surplus research carries over to the next tech.
int ResearchCarriedOver(int accumulated, int cost);

// Undiscovered techs whose every prerequisite is known.
std::vector<const TechConfig_t*> AvailableTechs(const ResearchManager& rResearch);

} // namespace ac
