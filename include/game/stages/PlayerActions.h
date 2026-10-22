#pragma once

#include "game/stages/YieldingPerFactionTurnStage.h"
#include "game/base/BaseTypes.h"
#include "game/units/Unit.h"
#include <unordered_set>

namespace ac
{

class PlayerActions : public YieldingPerFactionTurnStage
{
public:
    explicit PlayerActions(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
    void OnExitImpl() override;
    void OnResetPassState_() override;

private:
    // The player's first call yields; End Turn resumes and resolves orders, yielding again while a
    // unit needs orders. A completed pass continues, unordered units or not.
    enum class Phase_t
    {
        AwaitingInteraction,
        EndingInteraction
    };

    Phase_t m_phase = Phase_t::AwaitingInteraction;
    // Units whose multi-turn order already advanced this pass (Continue). Complete/Expended
    // units are not recorded so a new order assigned after a mid-pass Yield can still run.
    std::unordered_set<UnitId_t> m_advancedUnitIds;

};

} // namespace ac
