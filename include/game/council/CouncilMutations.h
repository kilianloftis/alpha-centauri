#pragma once

#include <vector>

namespace ac
{

class Faction;
class GameState;
struct CouncilProposalConfig_t;
struct CouncilRulesConfig_t;

// A passed proposal's on_passed_effects, with every member a subject: GrantEnergy credits each
// of them, which is what "+500 energy credits for each Council member" means.
void ApplyPassedCouncilEffects(GameState& rGameState, const std::vector<Faction*>& rMembers,
                               const CouncilProposalConfig_t& rConfig);

// The rules' on_elected_effects for a faction taking office. Continuous governor effects stay
// query-time through CouncilEffects.
void ApplyCouncilGovernor(GameState& rGameState, Faction& rGovernor,
                          const CouncilRulesConfig_t& rRules);

} // namespace ac
