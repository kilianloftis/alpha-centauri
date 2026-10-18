#include "game/council/CouncilMutations.h"

#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/Faction.h"

namespace ac
{

void ApplyPassedCouncilEffects(GameState& rGameState, const std::vector<Faction*>& rMembers,
                               const CouncilProposalConfig_t& rConfig)
{
    TriggeredEffectContext_t context(rGameState, rMembers);
    ApplyTriggeredEffects(rConfig.onPassedEffects, context);
}

void ApplyCouncilGovernor(GameState& rGameState, Faction& rGovernor,
                          const CouncilRulesConfig_t& rRules)
{
    TriggeredEffectContext_t context(rGameState, rGovernor);
    ApplyTriggeredEffects(rRules.onElectedEffects, context);
}

} // namespace ac
