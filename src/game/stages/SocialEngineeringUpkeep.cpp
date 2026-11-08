#include "game/stages/SocialEngineeringUpkeep.h"

#include "game/faction/Faction.h"
#include "game/social-engineering/SocialEngineeringMutations.h"
#include "game/stages/TurnStageRegistrar.h"

namespace ac
{

namespace { TurnStageRegistrar<SocialEngineeringUpkeep> g_registrar("SocialEngineeringUpkeep"); }

SocialEngineeringUpkeep::SocialEngineeringUpkeep(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t SocialEngineeringUpkeep::ExecuteImpl(GameState& /*rGameState*/, Faction& rFaction)
{
    CommitSocialEngineering(rFaction);
    return StageResult_t::Continue;
}

} // namespace ac
