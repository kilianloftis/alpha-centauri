#pragma once

#include <vector>

namespace ac
{

class Faction;
class GameState;
struct CouncilProposalConfig_t;
struct CouncilRulesConfig_t;

// Applies the game-state mutations a passed council proposal produces: a proposal's
// Instantaneous effects (energy grants; world-parameter changes trigger world events)
// and the Planetary Governor's diplomatic privileges. Keeps this outward-facing mutation
// out of the council's voting logic.
class CouncilOutcomeApplier
{
public:
    explicit CouncilOutcomeApplier(const CouncilRulesConfig_t& rRules);

    // Apply a passed proposal's Instantaneous effects across the council members.
    void ApplyInstantaneousEffects(const std::vector<Faction*>& rMembers,
                                   const CouncilProposalConfig_t& rConfig);

    // Grant the governor their configured diplomatic privileges over the other members.
    void ApplyGovernor(GameState& rGameState,
                       const std::vector<Faction*>& rMembers,
                       const Faction& rGovernor);

private:
    const CouncilRulesConfig_t& m_rRules;
};

} // namespace ac
