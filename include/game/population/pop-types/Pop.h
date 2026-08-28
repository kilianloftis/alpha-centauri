#pragma once

#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseTypes.h"
#include "game/map/WorkedTileIndex.h"

#include <vector>

namespace ac
{

class Tile;
struct PopTypeConfig_t;
// Forward-declared rather than including ActiveEffect.h: only the private helper below names
// it, and Pop.cpp has the full definition before any element is constructed.
struct ActiveEffect_t;

// A pop's contribution to the two mood sums. Returned together because both come from one
// pass over this pop type's ThisPop effects — resolving them separately collected and filtered
// the same list twice per pop, on a path that runs for every pop of every base each turn.
struct MoodWeights_t
{
    int riot = 0;
    int goldenAge = 0;
};

// Specialist generation output (not tile-based)
struct SpecialistOutput_t
{
    int econ;   // Economic output
    int labs;   // Research output
    int psych;  // Psych output
};

// A single population unit. Behaviour is entirely driven by its PopTypeConfig_t.
class Pop
{
public:
    explicit Pop(const PopTypeConfig_t& rConfig);

    // Type id string matching the config (e.g. "Worker", "Librarian")
    const char* GetPopType() const;

    // The full config backing this pop's current type.
    const PopTypeConfig_t& GetConfig() const;

    // True if this pop can work a tile (can_work_tile in config). A capability, not a class:
    // plain workers, drones and talents all answer true.
    bool IsWorker() const;

    // Position in the promotion graph, derived at load. These partition every pop: exactly one
    // is true, with types outside the graph (specialists) answering false to all three.
    bool IsDrone() const;
    bool IsTalent() const;
    bool IsPlainWorker() const;

    // A type the player picked deliberately, so composition must not convert it and pop loss
    // takes it last. Default workers are assignable but not protected — they are what
    // composition converts from. See docs/game-rules-decisions.md §1.
    bool IsPlayerChoiceType() const;

    // In the promotion graph at all — the pool composition seats into, and the population the
    // mood sums range over. False for specialists.
    bool IsInCompositionGraph() const;

    // True when composition may reshape this pop: in the promotion graph, and not a
    // player-choice type.
    bool ParticipatesInComposition() const;

    // Mood contributions, resolved from this pop type's own ThisPop effects at seed 0. Types
    // that declare neither contribute 0 to both, which is how specialists stay out of the riot
    // and golden age calculations without a special case.
    MoodWeights_t GetMoodWeights() const;

    // Drone pressure this body absorbs (Drone 1, SuperDrone 2; 0 for everything else).
    int GetDroneWeight() const;


    // Swap this pop's type config in-place.
    // Clears tile assignment if the new type is not a worker.
    void Convert(const PopTypeConfig_t& rConfig);

    // Tile assignment (only meaningful when IsWorker() is true).
    // The claim is minted by WorkedTileIndex::TryClaim (reached through
    // WorkerAssignmentManager) and releases its tile automatically when this pop dies,
    // converts to a non-worker type, or is reassigned. Pass an empty claim to unassign.
    // Throws if a non-empty claim is given to a pop type that cannot work tiles.
    void SetTileClaim(WorkedTileClaim claim);
    const Tile* GetTile() const;

    // True if the player assigned this pop's tile (protected from auto-assignment).
    // Stored on the claim, so it cannot outlive the assignment.
    bool IsUserAssigned() const;

    // Apply this pop's tile multipliers to raw tile resources.
    // Returns modified resources based on pop type's tile_multipliers config.
    // Only meaningful when IsWorker() is true.
    TileResources_t ApplyTileMultipliers(const TileResources_t& resources) const;

    // Get specialist generation output (econ/labs/psych).
    // This is direct output from specialists, not from tiles.
    SpecialistOutput_t GetSpecialistOutput() const;

private:
    // This pop type's own effects for one scope, materialized so a caller can resolve several
    // stats against it without re-collecting.
    std::vector<ActiveEffect_t> CollectScoped_(EffectScope_t scope) const;

    const PopTypeConfig_t* m_pConfig;
    WorkedTileClaim m_tileClaim;
};

} // namespace ac
