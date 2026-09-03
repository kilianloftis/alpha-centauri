#include "game/stages/Mood.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BaseMoodEffects.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/TurnStageRegistrar.h"

namespace ac
{

namespace { TurnStageRegistrar<Mood> g_registrar("Mood"); }

Mood::Mood(HookContext hookContext)
    : PerFactionTurnStage(std::move(hookContext))
{
}

void Mood::OnEnterImpl()
{
    m_committedBaseIds.clear();
}

void Mood::OnExitImpl()
{
    m_committedBaseIds.clear();
}

StageResult_t Mood::ExecuteImpl(GameState& rGameState, Faction& rFaction)
{
    for (const BaseId_t baseId : SnapshotFactionBaseIds_(rFaction))
    {
        BaseManager* pBase = rFaction.FindBase(baseId);
        if (!pBase || pBase->GetPopulation().GetSize() == 0)
        {
            continue;
        }
        if (!m_committedBaseIds.insert(baseId).second)
        {
            continue;
        }

        CommitBaseMood_(*pBase);
        DispatchRiotTierOnEnter_(rGameState, *pBase);
    }

    return StageResult_t::Continue;
}

std::vector<BaseId_t> Mood::SnapshotFactionBaseIds_(const Faction& rFaction)
{
    std::vector<BaseId_t> baseIds;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        baseIds.push_back(rBase.GetBaseId());
    }
    return baseIds;
}

void Mood::CommitBaseMood_(BaseManager& rBase)
{
    PopulationManager& rPopulation = rBase.GetPopulation();
    rPopulation.EnsureCompositionCurrent();
    rPopulation.CommitMood();
}

void Mood::DispatchRiotTierOnEnter_(GameState& rGameState, BaseManager& rBase)
{
    const RiotTier_t* pTier = ActiveRiotTierFor(rBase);
    if (!pTier || pTier->onEnterEffects.empty())
    {
        return;
    }

    DispatchInstantaneousEffects(pTier->onEnterEffects, rBase, rGameState);
}

} // namespace ac
