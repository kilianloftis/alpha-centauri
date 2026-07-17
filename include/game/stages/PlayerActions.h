#pragma once

#include "game/TurnStages.h"

namespace ac
{

class Unit;

class PlayerActions : public PerFactionTurnStage
{
public:
    explicit PlayerActions(HookContext hookContext);
    ~PlayerActions() = default;

protected:
    StageResult_t ExecuteImpl(GameState& rGameState, Faction& rFaction) override;

private:
    // Player-faction re-entrancy: first call always Yields for interaction. Enter resumes
    // and resolves pending orders; if a unit finishes still needing orders, Yield again.
    // Once the pass completes, Continue — Enter ends the phase even if other units were
    // left unordered.
    enum class Phase_t
    {
        AwaitingInteraction,
        EndingInteraction
    };

    Phase_t m_phase = Phase_t::AwaitingInteraction;

    static bool DoesUnitRequireOrders_(const Unit& rUnit);
};

} // namespace ac
