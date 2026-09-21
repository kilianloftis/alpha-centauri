#include "game/council/CouncilOutcomeApplier.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/effects/TriggeredEffectDispatch.h"

namespace ac
{

CouncilOutcomeApplier::CouncilOutcomeApplier(const CouncilRulesConfig_t& rRules)
    : m_rRules(rRules)
{
}

void CouncilOutcomeApplier::ApplyPassedEffects(GameState& rGameState,
                                               const std::vector<Faction*>& rMembers,
                                               const CouncilProposalConfig_t& rConfig)
{
    // Every member is a subject: GrantEnergy credits each of them, which is what
    // "+500 energy credits for each Council member" means.
    TriggeredEffectContext_t context(rGameState, rMembers);
    ApplyTriggeredEffects(rConfig.onPassedEffects, context);
}

void CouncilOutcomeApplier::ApplyGovernor(GameState& rGameState,
                                          const std::vector<Faction*>& /*rMembers*/,
                                          Faction& rGovernor)
{
    TriggeredEffectContext_t context(rGameState, rGovernor);
    ApplyTriggeredEffects(m_rRules.onElectedEffects, context);
}

} // namespace ac
