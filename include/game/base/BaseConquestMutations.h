#pragma once

#include "game/base/BaseConquestConfig.h"

#include <random>

namespace ac
{

class Unit;
class BaseManager;
class Faction;
class GameState;
class Tile;

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

// After the last owner unit on a base tile dies: last-defender pop loss, then a native raid if
// one applies. Capture needs entry.
BaseConquestResult_t ResolvePostCombatBaseConquest(Unit& rAttacker,
                                                   const Tile& rDefenderTile,
                                                   GameState& rGameState,
                                                   std::mt19937& rRng);

// Unit entered a foreign base tile with no garrison: capture (!CannotCaptureBases) or raid.
BaseConquestResult_t ResolveBaseEntryConquest(Unit& rMover,
                                              GameState& rGameState,
                                              std::mt19937& rRng);

// rCaptor takes rBase by force, as capture_base does for conquest and mind control. When
// HeadquartersEscapeDestination names a base, an AI owner moves its Headquarters there at once
// and the player is told; a player owner is asked.
void TakeBase(GameState& rGameState, BaseManager& rBase, Faction& rCaptor);

// rOwner pays headquarters_escape's cost and builds its Headquarters at rDestination. Throws
// unless MayEvacuateHeadquarters.
void EvacuateHeadquarters(GameState& rGameState, Faction& rOwner, BaseManager& rDestination);

} // namespace ac
