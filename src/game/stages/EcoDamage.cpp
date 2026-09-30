#include "game/stages/EcoDamage.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/TurnStageRegistrar.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
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
    const EcoDamageConfig_t& rConfig = *rFaction.GetDataContext().ecoDamageConfig;
    std::mt19937& rRng = rGameState.GetRng();

    std::vector<BaseId_t> baseIds;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        baseIds.push_back(rBase.GetBaseId());
    }
    for (const BaseId_t baseId : baseIds)
    {
        BaseManager* pBase = rFaction.FindBase(baseId);
        if (!pBase)
        {
            continue;
        }
        const int chance = std::min(rConfig.maxChancePercent, pBase->GetEcologicalDamage());
        if (chance <= 0)
        {
            continue;
        }
        if (std::uniform_int_distribution<int>(0, 99)(rRng) >= chance)
        {
            continue;
        }
        if (Tile* pPopTile = PickPopTile_(rGameState, *pBase, rRng))
        {
            Pop_(rGameState, *pBase, *pPopTile);
        }
    }
    return StageResult_t::Continue;
}

Tile* EcoDamage::PickPopTile_(GameState& rGameState, const BaseManager& rBase,
                              std::mt19937& rRng)
{
    WorldMap& rMap = rGameState.GetWorldMap();
    std::vector<Tile*> candidates;
    for (const Tile* pTile : rBase.GetWorkerAssignments().GetWorkableTiles())
    {
        if (pTile->HasImprovement(ImprovementIds::k_Base)
            || pTile->HasTerrainFeature(ImprovementIds::k_Fungus))
        {
            continue;
        }
        candidates.push_back(rMap.GetTile(pTile->GetX(), pTile->GetY()));
    }
    if (candidates.empty())
    {
        return nullptr;
    }
    std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
    return candidates[pick(rRng)];
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
    ApplyTriggeredEffects(rFaction.GetDataContext().ecoDamageConfig->onPopEffects, context);
}

} // namespace ac
