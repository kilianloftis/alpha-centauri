#include "game/stages/EcoDamage.h"

#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/stages/TurnStageRegistrar.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/ecology/EcologyRules.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/base/BaseManager.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "lib/EventBus.h"
#include "lib/RandomRoll.h"
#include "game/ecology/EcologyLedger.h"

#include <algorithm>
#include <vector>

namespace ac
{

namespace { TurnStageRegistrar<EcoDamage> g_registrar("EcoDamage"); }

EcoDamage::EcoDamage(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

StageResult_t EcoDamage::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    std::mt19937& rRng = rGameState.GetRng();

    std::vector<BaseId_t> baseIds;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        baseIds.push_back(rBase.GetBaseId());
    }
    for (const BaseId_t baseId : baseIds)
    {
        if (BaseManager* pBase = rFaction.FindBase(baseId))
        {
            MaybePopFungus_(rGameState, *pBase, rRng);
        }
    }
    return StageResult_t::Continue;
}

// Rolls the base's fungal pop chance and, on a hit, pops one of its workable tiles.
void EcoDamage::MaybePopFungus_(GameState& rGameState, BaseManager& rBase, std::mt19937& rRng)
{
    if (!RollPercent(FungalPopChance(rBase), rRng))
    {
        return;
    }
    if (Tile* pPopTile = PickPopTile_(rGameState, rBase, rRng))
    {
        Pop_(rGameState, rBase, *pPopTile);
    }
}

Tile* EcoDamage::PickPopTile_(GameState& rGameState, const BaseManager& rBase,
                              std::mt19937& rRng)
{
    const std::vector<const Tile*> candidates = FungalPopTiles(rBase);
    if (candidates.empty())
    {
        return nullptr;
    }
    const Tile& rPicked = *candidates[PickUniformIndex(candidates.size(), rRng)];
    return rGameState.GetWorldMap().GetTile(rPicked.GetCoord());
}

void EcoDamage::Pop_(GameState& rGameState, BaseManager& rBase, Tile& rPopTile)
{
    Faction& rFaction = rBase.GetFaction();
    rGameState.GetEcologyLedger().RecordFungalBloom(rFaction.GetFactionId());
    rGameState.GetEventBus().Publish(EvFungalBloom{rFaction.GetFactionId(), rBase.GetBaseId()});
    if (rFaction.IsPlayerControlled())
    {
        EnqueueForPlayer(rGameState,
                         NoticeInteraction_t{
                             PauseOnEventId_t::FungalBloom,
                             "Fungal Bloom",
                             "Ecological damage near " + rBase.GetName()
                                 + " has triggered a fungal bloom.",
                             rPopTile.GetCoord(),
                         });
    }

    TriggeredEffectContext_t context(rGameState, rBase);
    context.pTile = &rPopTile;
    context.pRng = &rGameState.GetRng();
    ApplyTriggeredEffects(rGameState.GetGameData().ecoDamageConfig->onPopEffects, context);
}

} // namespace ac
