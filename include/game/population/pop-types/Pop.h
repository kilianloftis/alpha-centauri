#pragma once

#include "game/faction/base/BaseTypes.h"
#include "game/map/WorkedTileIndex.h"

namespace ac
{

class Tile;
struct PopTypeConfig_t;

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

    // True if this pop can work a tile (can_work_tile in config). A capability, not a role:
    // plain workers, drones and talents all answer true.
    bool IsWorker() const;

    // Role predicates. Together with IsPlainWorker below these partition every pop: exactly one
    // of the four is true. The role comes from the config's `role` field.
    bool IsDrone() const;
    bool IsTalent() const;
    bool IsSpecialist() const;

    // Worker that is neither drone nor talent — the pool composition converts to/from.
    // The fourth member of the partition above.
    bool IsPlainWorker() const;

    // True if the player can manually assign this pop type
    bool IsPlayerAssignable() const;

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
    const PopTypeConfig_t* m_pConfig;
    WorkedTileClaim m_tileClaim;
};

} // namespace ac
