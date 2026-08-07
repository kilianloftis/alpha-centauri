#pragma once

#include "lib/GameEvent.h" // BaseId_t
#include "game/units/MoraleCalculator.h"
#include "game/units/ProbeActionConfig.h"
#include "game/units/ProbeActionResult.h"
#include "game/units/ProbeTarget.h"
#include "game/units/Unit.h"

#include <optional>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

class WorldMap;
class GameState;
class Tile;
struct GameDataContext;

// Orchestrates probe missions: order-domain gates (moves, adjacency), energy payment,
// chance/roll setup, effect application, promote, and probe destruction.
// Pure eligibility/math live in ProbeRules; world mutations live in ProbeActionEffects.
//
// Targets are addressed by tile and resolved per call (see ProbeTarget.h), so callers
// never hold a base or unit pointer across turns or UI interactions.
class ProbeActionExecutor
{
public:
    ProbeActionExecutor(WorldMap& rWorldMap, const MoraleCalculator& rMorale, std::mt19937& rRng);

    // Order-domain gates plus CanProbeAction mechanics for one configured action against
    // whatever rAction's target kind resolves to on rTile.
    bool CanTryProbeAction(const Unit& rUnit, const ProbeActionConfig_t& rAction,
                           const Tile& rTile, GameState& rGameState) const;

    // True when the probe may open the action menu against rTile (at least one legal action).
    bool CanOpenProbeActions(const Unit& rProbe, const Tile& rTile, GameState& rGameState,
                             const GameDataContext& rDataContext) const;

    // Legal (id, display name) pairs for the probe against rTile.
    std::vector<std::pair<ProbeActionId_t, std::string>> ListAvailableProbeActions(
        const Unit& rProbe, const Tile& rTile, GameState& rGameState,
        const GameDataContext& rDataContext) const;

    // Pay energy when required, roll mission/escape, apply the action effect, promote XP on
    // full success, and DestroyUnit the probe on mission failure or failed escape.
    // Whether this is a *repeat* attempt (and so uses the action's risk_repeat) is decided
    // here, not passed in: it is session history, which the executor owns. It used to be a
    // caller-supplied flag defaulting to false that no caller ever set, so risk_repeat was
    // dead config.
    ProbeActionResult_t TryProbeAction(Unit& rUnit, ProbeActionId_t actionId, const Tile& rTile,
                                       GameState& rGameState, const GameDataContext& rDataContext,
                                       const BuildingId_t& facilityId = {});

private:
    bool CanTryProbeAction_(const Unit& rUnit, const ProbeActionConfig_t& rAction,
                            const ProbeTarget_t& rTarget) const;

    std::optional<ProbeTarget_t> ResolveEligibleTarget_(
        const Unit& rUnit, const Tile& rTile, const ProbeActionConfig_t& rAction,
        GameState& rGameState) const;
    bool TryPayProbeCost_(Unit& rUnit, const ProbeActionConfig_t& rAction,
                          const ProbeTarget_t& rTarget);
    int ResolveMissionRisk_(const ProbeActionConfig_t& rAction, const ProbeTarget_t& rTarget,
                            const BuildingId_t& facilityId, bool bRepeatAtBase) const;
    void FillProbeChances_(ProbeActionResult_t& rResult, const Unit& rUnit,
                           const ProbeTarget_t& rTarget, const GameDataContext& rDataContext,
                           int risk) const;
    ProbeActionResult_t ResolveProbeRoll_(Unit& rUnit, const ProbeActionConfig_t& rAction,
                                          const ProbeTarget_t& rTarget, GameState& rGameState,
                                          const GameDataContext& rDataContext,
                                          const BuildingId_t& facilityId,
                                          ProbeActionResult_t result,
                                          const ProbeRollResult_t& roll);
    // Has rActorFactionId already run a probe action against this base? Records the attempt as
    // a side effect, so the first call for a pair is false and every later one is true.
    // TODO: the scope of "repeat" is not specified by any rule we have. This treats it as
    // permanent, per (faction, base), and shared across actions — so a prior drain_energy
    // marks a first-ever steal_tech as a repeat, even though risk_repeat is declared
    // per-action in config. Whether it should decay over turns, reset when the base changes
    // hands, or be tracked per action all need the SMAC rule.
    bool NoteAndCheckRepeatAtBase_(FactionId_t actorFactionId, BaseId_t targetBaseId);

    ProbeActionResult_t FailMission_(Unit& rUnit, ProbeActionResult_t result) const;
    ProbeActionResult_t CaptureProbe_(Unit& rUnit, ProbeActionResult_t result) const;
    void FinalizeProbeSuccess_(Unit& rUnit, ProbeActionResult_t& rResult) const;

    WorldMap& m_rWorldMap;
    const MoraleCalculator& m_rMorale;
    std::mt19937& m_rRng;
    // (actor faction, target base) pairs that have already been probed this session.
    std::set<std::pair<FactionId_t, BaseId_t>> m_probedBases;
};

} // namespace ac
