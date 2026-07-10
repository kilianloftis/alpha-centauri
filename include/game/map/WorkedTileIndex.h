#pragma once

#include "lib/Revision.h"
#include <functional>
#include <unordered_map>

namespace ac
{

class Tile;
class WorkedTileIndex;

// Invoked after a claim is displaced (see WorkedTileIndex::ClaimDisplacing) so the previous
// owner can react — a WorkerAssignmentManager re-runs auto-assignment to move the displaced
// worker to its best remaining tile. Must remain valid for the claim's whole lifetime.
using DisplacedWorkerHandler = std::function<void()>;

// Move-only handle to a worked-tile claim in WorkedTileIndex. Holding the claim IS the
// worker assignment: it records which tile is worked and whether the player made the
// assignment, and it releases the tile in the index when destroyed, overwritten, or
// cleared. Only WorkedTileIndex::TryClaim/ClaimDisplacing can create a non-empty claim,
// so a tile can never be worked twice — the invariant is structural, not a convention.
// The index must outlive every claim: WorldMap (which owns the index) is destroyed after
// all factions/bases/pops — see GameState's member ordering.
class WorkedTileClaim
{
public:
    WorkedTileClaim() = default;
    ~WorkedTileClaim();

    WorkedTileClaim(WorkedTileClaim&& rOther) noexcept;
    WorkedTileClaim& operator=(WorkedTileClaim&& rOther) noexcept;
    WorkedTileClaim(const WorkedTileClaim&) = delete;
    WorkedTileClaim& operator=(const WorkedTileClaim&) = delete;

    // The claimed tile, or nullptr for an empty (unassigned) claim.
    const Tile* GetTile() const { return m_pTile; }

    // True if the player made this assignment (protects it from auto-assignment churn).
    // Always false for an empty claim — the flag cannot outlive the assignment.
    bool IsUserAssigned() const { return m_bUserAssigned; }

private:
    friend class WorkedTileIndex;
    WorkedTileClaim(WorkedTileIndex& rIndex, const Tile& rTile, bool bUserAssigned,
                    DisplacedWorkerHandler onDisplaced);

    void Release_();
    void MoveFrom_(WorkedTileClaim& rOther) noexcept;

    WorkedTileIndex* m_pIndex = nullptr;
    const Tile* m_pTile = nullptr;
    bool m_bUserAssigned = false;
    // Non-null marks the claim displaceable (a pop's assignment); base-tile claims carry
    // no handler and can never be displaced.
    DisplacedWorkerHandler m_onDisplaced;
};

// World-scoped owner of the "which tiles are currently worked" state — the single point
// that enforces the one-worker-per-tile rule across ALL bases and factions (two nearby
// bases, friendly or enemy, may never work the same tile). Owned by WorldMap, next to
// UnitPositionIndex. Mutation happens exclusively through TryClaim/ClaimDisplacing and
// claim release, so there is exactly one place to extend when assignments must be
// repaired (e.g. a tile turning hostile) or reconciled after a future save-game load.
class WorkedTileIndex
{
public:
    WorkedTileIndex() = default;
    ~WorkedTileIndex() = default;

    // Non-movable: outstanding claims hold a pointer back to this index.
    WorkedTileIndex(const WorkedTileIndex&) = delete;
    WorkedTileIndex& operator=(const WorkedTileIndex&) = delete;

    // True if any pop of any base — this faction's or another's — is working rTile,
    // or a base occupies it.
    bool IsWorked(const Tile& rTile) const;

    // Claim rTile for a worker. Returns an empty claim (GetTile() == nullptr) if the tile
    // is already worked — an expected outcome while probing tiles during assignment, not
    // an error. onDisplaced, when provided, makes the claim displaceable by a base
    // founding (see ClaimDisplacing) and is invoked after the displacement.
    WorkedTileClaim TryClaim(const Tile& rTile, bool bUserAssigned,
                             DisplacedWorkerHandler onDisplaced = {});

    // Claim rTile for a base's own tile, displacing the worker currently on it (if any):
    // the worker's claim is emptied and its DisplacedWorkerHandler is invoked AFTER the
    // new claim is registered, so the previous owner's re-assignment can never take this
    // tile back. The returned claim carries no handler and is itself not displaceable.
    // Throws if rTile is already a base's own tile — a founding flow must never allow
    // founding on top of an existing base.
    WorkedTileClaim ClaimDisplacing(const Tile& rTile, bool bUserAssigned);

    // Bumped on every claim and release; consumed by derived-state caches (see lib/Revision.h).
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    friend class WorkedTileClaim;
    void Release_(const Tile& rTile);

    // Values point at the live claim objects (kept current by WorkedTileClaim's moves) so
    // ClaimDisplacing can find and empty the claim occupying a tile.
    std::unordered_map<const Tile*, WorkedTileClaim*> m_claims;
    Revision m_revision;
};

} // namespace ac
