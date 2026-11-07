#pragma once

#include "game/research/TechConfigParser.h"
#include "game/research/TechCostConfig.h"

#include <string>
#include <vector>

namespace ac
{

class Faction;
class IEffectsProvider;
class ResearchManager;
class TechCostCalculator;

// What tech cost_formula reads. pEffectsProvider is the faction's effect pool, and may be null
// for a manager built outside a faction.
TechCostInputs_t TechCostInputs(const ResearchManager& rResearch,
                                const IEffectsProvider* pEffectsProvider);

// The Lua formula: terranx tech_rate up to its TECHCOST and stagnation scales. The same for
// every tech, so it can be fixed at a breakthrough before the next target is picked.
int ResearchFormulaCost(const ResearchManager& rResearch,
                        const IEffectsProvider* pEffectsProvider,
                        const TechCostCalculator& rCalculator);

// formulaCost times TechCost (faction, rTech's ThisTech, world_rules.json), truncated.
int ScaledTechCost(int formulaCost, const IEffectsProvider* pEffectsProvider,
                   const TechConfig_t& rTech);

// Undiscovered techs whose every prerequisite is known.
std::vector<const TechConfig_t*> AvailableTechs(const ResearchManager& rResearch);

// tech_recurse: 1 plus the deepest prerequisite's level, so a root tech is 1.
int TechLevel(const TechId& rTechId);

// The deepest TechLevel among rResearch's discovered techs; 0 with none.
int HighestTechLevel(const ResearchManager& rResearch);

// True when any non-native-life faction has discovered one of rTechIds. Empty rTechIds is true
// (no tech gate).
bool AnyFactionHasTech(const std::vector<std::string>& rTechIds);

} // namespace ac
