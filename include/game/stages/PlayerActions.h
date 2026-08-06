#pragma once

#include "game/TurnStages.h"
#include "game/faction/base/BaseTypes.h"
#include "game/units/Unit.h"
#include <optional>
#include <unordered_set>

namespace ac
{

class PlayerActions : public PerFactionTurnStage
{
public:
    explicit PlayerActions(HookContext hookContext);

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;
    void OnExitImpl() override;

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
    // Faction whose pass-local state (phase / advanced set) is active. Reset when Execute
    // is entered for a different faction (e.g. resume faction eliminated while yielded).
    std::optional<FactionId_t> m_activeFactionId;
    // Units whose multi-turn order already advanced this pass (Continue). Complete/Expended
    // units are not recorded so a new order assigned after a mid-pass Yield can still run.
    std::unordered_set<UnitId_t> m_advancedUnitIds;

    void ResetPassState_();
    static bool DoesUnitRequireOrders_(const Unit& rUnit);
};

} // namespace ac
