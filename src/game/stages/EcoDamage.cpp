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

// Rolls the base's fungal pop chance and, on a hit, pops a tile next to it.
void EcoDamage::MaybePopFungus_(GameState& rGameState, BaseManager& rBase, std::mt19937& rRng)
{
    const int chance = FungalPopChance(rBase);
    if (chance <= 0 || std::uniform_int_distribution<int>(0, 99)(rRng) >= chance)
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
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    const Tile& rPicked = *candidates[pick(rRng)];
    return rGameState.GetWorldMap().GetTile(rPicked.GetX(), rPicked.GetY());
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
                             TileCoord_t{rPopTile.GetX(), rPopTile.GetY()},
                         });
    }

    TriggeredEffectContext_t context(rGameState, rBase);
    context.pTile = &rPopTile;
    context.pRng = &rGameState.GetRng();
    ApplyTriggeredEffects(rGameState.GetGameData().ecoDamageConfig->onPopEffects, context);
}

} // namespace ac
