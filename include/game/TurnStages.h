#pragma once

#include "game/HookContext.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <magic_enum.hpp>

namespace ac
{

class GameState;
class Faction;

enum class TurnStage
{
    TurnStart,
    NewYearBegins,
    ResourceCollection,
    // Income_Nutrients,
    // Income_Minerals,
    // Income_Energy,
    // Income_Commerce,
    // EnergyAllocation_Economy,
    // EnergyAllocation_Labs,
    // EnergyAllocation_Psych,
    ResearchAccumulation,
    // Research_LabOutput,
    // Research_TechDiscovery,
    BaseProduction,
    // Production_Facilities,
    // Production_Units,
    // Production_Terraforming,
    // Production_SecretProjects,
    Population,
    // Population_SurplusDeficit,
    // Population_DroneTalentBalance,
    Upkeep,
    // Upkeep_UnitSupport,
    // Upkeep_FacilityCosts,
    PlayerActions,
    WorldEvents,
    // WorldEvents_Mindworms,
    // WorldEvents_Fungus,
    // WorldEvents_Disasters,
    // WorldEvents_MonolithEffects,
    VictoryConditionChecks,
    // Victory_ConditionChecks,
    TurnEnd,
    Save,
    
    Count
};

struct TurnStageInfo
{
    std::string id;
    std::string description;
    bool repeat_for_each_faction;
};

class TurnStageBase
{
public:
    TurnStageBase(std::shared_ptr<HookContext> pHookContext)
    : m_pHookContext(pHookContext)
    {}
    
    virtual ~TurnStageBase() = default;
    
    virtual void Execute(GameState* pGameState = nullptr, Faction* pFaction = nullptr)
    {
        if (m_pHookContext && m_pHookContext->HasReplaceHooks())
        {
            m_pHookContext->ExecuteReplaceHooks();
        }
        else
        {
            Execute_(pGameState, pFaction);
        }
    }
    
    void OnEnter()
    {
        if (m_pHookContext)
        {
            m_pHookContext->ExecutePreHooks();
        }
    }
    
    void OnExit()
    {
        if (m_pHookContext)
        {
            m_pHookContext->ExecutePostHooks();
        }
    }

private:

    virtual void Execute_(GameState* pGameState = nullptr, Faction* pFaction = nullptr) = 0;

protected:
    std::shared_ptr<HookContext> m_pHookContext;
};

using TurnStageRegistry_t = std::map<TurnStage, std::unique_ptr<TurnStageBase>>;

} // namespace ac
