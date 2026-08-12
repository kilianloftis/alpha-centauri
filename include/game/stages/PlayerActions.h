#pragma once

#include "game/stages/YieldingPerFactionTurnStage.h"
#include "game/faction/base/BaseTypes.h"
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
    // Player-faction re-entrancy: first call always Yields for interaction. Enter / End Turn
    // resumes and resolves pending orders; if a unit finishes still needing orders, Yield
    // again. Once the pass completes, Continue — ending the phase even if other units were
    // left unordered.
    enum class Phase_t
    {
        AwaitingInteraction,
        EndingInteraction
    };

    Phase_t m_phase = Phase_t::AwaitingInteraction;
    // Units whose multi-turn order already advanced this pass (Continue). Complete/Expended
    // units are not recorded so a new order assigned after a mid-pass Yield can still run.
    std::unordered_set<UnitId_t> m_advancedUnitIds;

    static bool DoesUnitRequireOrders_(const Unit& rUnit);
};

} // namespace ac
