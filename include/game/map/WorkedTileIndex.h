#pragma once

#include "lib/Revision.h"
#include <functional>
#include <unordered_map>

namespace ac
{

class Tile;
class WorkedTileIndex;

// Called after a claim is displaced, so the owner can reassign; valid for the claim's life.
using DisplacedWorkerHandler = std::function<void()>;

// Holding the claim is the worker assignment, and only the index mints one, so no tile is
// worked twice. Released when destroyed or overwritten; the index must outlive it.
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

// Which tiles are worked, across every base and faction. Changes only through TryClaim,
// ClaimDisplacing and release.
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

    // Empty if the tile is already worked, an expected outcome while probing. onDisplaced makes
    // the claim displaceable by a founding.
    WorkedTileClaim TryClaim(const Tile& rTile, bool bUserAssigned,
                             DisplacedWorkerHandler onDisplaced = {});

    // Claims a base's own tile, displacing its worker after this claim is registered, so the
    // worker cannot take it back. Throws if it is already a base tile.
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
