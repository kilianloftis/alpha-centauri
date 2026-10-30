#pragma once

#include "game/combat/MoraleCalculator.h"
#include "game/probes/ProbeActionConfig.h"
#include "game/probes/ProbeActionResult.h"
#include "game/probes/ProbeTarget.h"
#include "game/units/Unit.h"
#include "game/probes/ProbeRules.h"

#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

class GameState;
class Tile;

// Runs probe missions: gates, payment, rolls, effects, promotion and probe loss. Targets are
// resolved per call from their tile, so no caller holds one across turns.
class ProbeActionExecutor
{
public:
    ProbeActionExecutor(const MoraleCalculator& rMorale, const ProbeActionsConfig_t& rProbeActions,
                        std::mt19937& rRng);

    // Order-domain gates plus CanProbeAction mechanics for one configured action against
    // whatever rAction's target kind resolves to on rTile.
    bool CanTryProbeAction(const Unit& rUnit, const ProbeActionConfig_t& rAction,
                           const Tile& rTile, GameState& rGameState) const;

    // True when the probe may open the action menu against rTile (at least one legal action,
    // or contact with the target would destroy the probe).
    bool CanOpenProbeActions(const Unit& rProbe, const Tile& rTile, GameState& rGameState) const;

    // True when an adjacent probe with moves remaining would die on contact with rTile's target
    // (ProbeContactDestroys): no pay, no roll, no menu.
    bool WouldContactDestroyProbe(const Unit& rProbe, const Tile& rTile,
                                  GameState& rGameState) const;

    // Legal (id, display name) pairs for the probe against rTile.
    std::vector<std::pair<ProbeActionId_t, std::string>> ListAvailableProbeActions(
        const Unit& rProbe, const Tile& rTile, GameState& rGameState) const;

    // The factions the probe may implicate in actionId against rTile's target: FrameCandidates
    // when ProbeMayFrame allows, else none.
    std::vector<FactionId_t> ListFrameCandidates(const Unit& rProbe, ProbeActionId_t actionId,
                                                 const Tile& rTile, GameState& rGameState) const;

    // Pays, rolls mission and escape, applies the effect, promotes on full success and destroys a
    // failed probe. framedId is the faction implicated; throws unless it is a FrameCandidates
    // entry for an action ProbeMayFrame allows.
    ProbeActionResult_t TryProbeAction(Unit& rUnit, ProbeActionId_t actionId, const Tile& rTile,
                                       GameState& rGameState, const BuildingId_t& facilityId,
                                       std::optional<FactionId_t> framedId);

private:
    bool CanTryProbeAction_(const Unit& rUnit, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget) const;

    std::optional<ProbeTarget_t> ResolveContactTarget_(const Unit& rUnit, const Tile& rTile,
                                                       GameState& rGameState) const;
    ProbeActionResult_t DestroyOnProbeContact_(Unit& rUnit, const ProbeTarget_t& rTarget,
                                               GameState& rGameState) const;

    std::optional<ProbeTarget_t> ResolveEligibleTarget_(
        const Unit& rUnit, const Tile& rTile, const ProbeActionConfig_t& rAction,
        GameState& rGameState) const;
    bool TryPayProbeCost_(Unit& rUnit, const ProbeActionConfig_t& rAction,
                          const ProbeTarget_t& rTarget);
    ProbeActionResult_t ResolveProbeRoll_(Unit& rUnit, const ProbeActionConfig_t& rAction,
                                          const ProbeTarget_t& rTarget, GameState& rGameState,
                                          const BuildingId_t& facilityId,
                                          std::optional<FactionId_t> framedId,
                                          ProbeActionResult_t result,
                                          const ProbeRollResult_t& roll);
    std::optional<ProbeFrame_t> ApplyProbeStanding_(const Unit& rUnit,
                                                    const ProbeActionConfig_t& rAction,
                                                    const ProbeTarget_t& rTarget,
                                                    GameState& rGameState,
                                                    std::optional<FactionId_t> framedId,
                                                    const ProbeRollResult_t& roll);

    ProbeActionResult_t FailMission_(Unit& rUnit, ProbeActionResult_t result) const;
    ProbeActionResult_t CaptureProbe_(Unit& rUnit, ProbeActionResult_t result) const;
    bool ReturnProbeHome_(Unit& rUnit, GameState& rGameState) const;
    void FinalizeProbeSuccess_(Unit& rUnit, ProbeActionResult_t& rResult) const;

    const MoraleCalculator& m_rMorale;
    const ProbeActionsConfig_t& m_rProbeActions;
    std::mt19937& m_rRng;
};

} // namespace ac
