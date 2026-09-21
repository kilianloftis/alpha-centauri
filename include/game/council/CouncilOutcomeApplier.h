#pragma once

#include <vector>

namespace ac
{

class Faction;
class GameState;
struct CouncilProposalConfig_t;
struct CouncilRulesConfig_t;

// Applies the game-state changes a council outcome produces: a passed proposal's
// on_passed_effects (energy grants; world-parameter changes trigger world events) and a new
// governor's on_elected_effects (e.g. a SetInfiltration that outlives the term). Keeps this
// outward-facing mutation out of the council's voting logic.
class CouncilOutcomeApplier
{
public:
    explicit CouncilOutcomeApplier(const CouncilRulesConfig_t& rRules);

    // Apply a passed proposal's one-shot outcomes across the council members. Needs a live
    // session: the triggered dispatcher resolves against GameState.
    void ApplyPassedEffects(GameState& rGameState, const std::vector<Faction*>& rMembers,
                            const CouncilProposalConfig_t& rConfig);

    // Apply the rules' on_elected_effects for a faction taking office. Continuous governor
    // effects (including Infiltration) stay query-time via CouncilEffects / HasInfiltration.
    void ApplyGovernor(GameState& rGameState,
                       const std::vector<Faction*>& rMembers,
                       Faction& rGovernor);

private:
    const CouncilRulesConfig_t& m_rRules;
};

} // namespace ac
