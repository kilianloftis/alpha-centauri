#include "game/supply/SupplyPodMutations.h"

#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/PlayerInteractionQueue.h"
#include "game/base/BaseManager.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TriggeredEffectDispatch.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/research/TechRegistry.h"
#include "game/supply/SupplyPodConfig.h"
#include "game/supply/SupplyPodIndex.h"
#include "game/supply/SupplyPodRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"

#include <stdexcept>
#include <string>
#include <variant>

namespace ac
{

namespace
{

void FillResultFromEffects_(SupplyPodOpenResult_t& rResult,
                            const std::vector<TriggeredEffectResult_t>& rEffects,
                            const TechRegistry* pTechs)
{
    for (const TriggeredEffectResult_t& rEffect : rEffects)
    {
        if (const auto* pEnergy = std::get_if<EnergyGranted_t>(&rEffect))
        {
            rResult.creditAmount = pEnergy->amount;
        }
        else if (const auto* pTech = std::get_if<TechGranted_t>(&rEffect))
        {
            if (pTechs)
            {
                if (const TechConfig_t* pConfig = pTechs->Find(pTech->techId))
                {
                    rResult.techName = pConfig->name;
                }
                else
                {
                    rResult.techName = pTech->techId;
                }
            }
            else
            {
                rResult.techName = pTech->techId;
            }
        }
        else if (const auto* pUnits = std::get_if<UnitsGranted_t>(&rEffect))
        {
            if (!pUnits->designId.empty())
            {
                rResult.unitName = pUnits->designId;
            }
        }
        else if (const auto* pOccupant = std::get_if<OccupantAdded_t>(&rEffect))
        {
            rResult.resourceBonusId = pOccupant->improvementId;
        }
        else if (const auto* pFinished = std::get_if<ProductionFinished_t>(&rEffect))
        {
            rResult.baseName = pFinished->baseName;
            rResult.itemName = pFinished->itemName;
            rResult.cameraTile = pFinished->cameraTile;
        }
        else if (const auto* pCommlink = std::get_if<CommlinkGranted_t>(&rEffect))
        {
            rResult.leaderTitle = pCommlink->leaderTitle;
            rResult.leaderName = pCommlink->leaderName;
            rResult.factionName = pCommlink->factionName;
        }
        else if (const auto* pTeleport = std::get_if<UnitTeleported_t>(&rEffect))
        {
            rResult.cameraTile = pTeleport->cameraTile;
        }
        else if (std::holds_alternative<UnitDestroyed_t>(rEffect))
        {
            rResult.bVisitorDestroyed = true;
        }
    }
}

void EnqueuePlayerNotice_(GameState& rGameState, const SupplyPodOpenResult_t& rResult)
{
    SupplyPodNoticeInteraction_t notice;
    notice.noticeKey = rResult.noticeKey;
    notice.imagePath = rResult.imagePath;
    notice.cameraTile = rResult.cameraTile;
    notice.creditAmount = rResult.creditAmount;
    notice.techName = rResult.techName;
    notice.unitName = rResult.unitName;
    notice.resourceBonusId = rResult.resourceBonusId;
    notice.baseName = rResult.baseName;
    notice.itemName = rResult.itemName;
    notice.leaderTitle = rResult.leaderTitle;
    notice.leaderName = rResult.leaderName;
    notice.factionName = rResult.factionName;
    EnqueueForPlayer(rGameState, std::move(notice));
}

} // namespace

SupplyPodOpenResult_t OpenSupplyPod(GameState& rGameState, Unit& rVisitor, std::mt19937& rRng)
{
    SupplyPodOpenResult_t result;
    Tile& rTile = rGameState.GetWorldMap().MutableTile(rVisitor.GetTile());
    if (!TileHasSupplyPod(rTile) || !CanOpenSupplyPod(rVisitor))
    {
        return result;
    }

    const SupplyPodConfig_t& rConfig = *rGameState.GetGameData().supplyPodConfig;
    SupplyPodIndex& rIndex = rGameState.GetSupplyPods();
    const SupplyPodEventGroupConfig_t& rEventGroup = EventGroupForPod(rConfig, rIndex, rTile);
    const int turn = rGameState.GetTurnNumber();
    const SupplyPodEventPick_t pick = PickEvent(rEventGroup, turn, rRng);

    rGameState.GetTileEffects().RemoveOccupantWithEffects(
        rTile, std::string(ImprovementIds::k_SupplyPod));
    rIndex.Clear(rTile);

    result.bOpened = true;
    result.eventId = pick.eventId;
    result.cameraTile = TileCoord_t{rTile.GetX(), rTile.GetY()};
    result.noticeKey = pick.pNotice->script;
    result.imagePath = pick.pNotice->image;

    if (pick.pEvent)
    {
        TriggeredEffectContext_t context(rGameState, rVisitor.GetFaction());
        context.pUnit = &rVisitor;
        context.pTile = &rTile;
        context.pFaction = &rVisitor.GetFaction();
        context.pBase = rVisitor.GetFaction().FindNearestBase(rTile);
        context.pRng = &rRng;

        const std::vector<TriggeredEffectResult_t> effectResults =
            ApplyTriggeredEffects(pick.pEvent->effects, context);
        FillResultFromEffects_(result, effectResults, rGameState.GetGameData().techRegistry.get());
    }

    if (rVisitor.GetFaction().IsPlayerControlled())
    {
        EnqueuePlayerNotice_(rGameState, result);
    }
    return result;
}

} // namespace ac
