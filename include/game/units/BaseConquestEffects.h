#pragma once

#include "game/units/BaseConquestConfig.h"

#include <random>

namespace ac
{

class Unit;
class BaseManager;
class GameState;
class Tile;
struct GameDataContext;

// Outcomes of a conquest-related mutation (for tests / UI hooks).
enum class BaseConquestOutcome_t
{
    None,
    LastDefenderCasualty,
    Captured,
    NativeRaid,
    Razed,
};

struct BaseConquestResult_t
{
    BaseConquestOutcome_t outcome = BaseConquestOutcome_t::None;
    int populationLost = 0;
    int facilitiesDestroyed = 0;
    int escapePodsSpawned = 0;
    bool bBaseRazed = false;
    // The raider spent itself on the raid and has been destroyed. Callers holding the unit
    // must stop using it — this is the only path here that frees the acting unit.
    bool bActorDestroyed = false;
};

// After the last owner unit on a base tile is destroyed: pop loss (unless
// PreventsConquestPopLoss), then native raid if applicable. Capture requires entry.
BaseConquestResult_t ResolvePostCombatBaseConquest(Unit& rAttacker,
                                                   const Tile& rDefenderTile,
                                                   GameState& rGameState,
                                                   const GameDataContext& rDataContext,
                                                   std::mt19937& rRng);

// Unit entered a foreign base tile with no garrison: capture (!CannotCaptureBases) or raid.
BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover,
                                              GameState& rGameState,
                                              const GameDataContext& rDataContext,
                                              std::mt19937& rRng);

} // namespace ac
