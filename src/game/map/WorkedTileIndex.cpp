#include "game/map/WorkedTileIndex.h"
#include "game/map/Tile.h"
#include <stdexcept>
#include <string>
#include <utility>

namespace ac
{

WorkedTileClaim::WorkedTileClaim(WorkedTileIndex& rIndex, const Tile& rTile, bool bUserAssigned,
                                 DisplacedWorkerHandler onDisplaced)
    : m_pIndex(&rIndex)
    , m_pTile(&rTile)
    , m_bUserAssigned(bUserAssigned)
    , m_onDisplaced(std::move(onDisplaced))
{
    m_pIndex->m_claims[m_pTile] = this;
}

WorkedTileClaim::~WorkedTileClaim()
{
    Release_();
}

WorkedTileClaim::WorkedTileClaim(WorkedTileClaim&& rOther) noexcept
{
    MoveFrom_(rOther);
}

WorkedTileClaim& WorkedTileClaim::operator=(WorkedTileClaim&& rOther) noexcept
{
    if (this != &rOther)
    {
        Release_();
        MoveFrom_(rOther);
    }
    return *this;
}

void WorkedTileClaim::MoveFrom_(WorkedTileClaim& rOther) noexcept
{
    m_pIndex = rOther.m_pIndex;
    m_pTile = rOther.m_pTile;
    m_bUserAssigned = rOther.m_bUserAssigned;
    m_onDisplaced = std::move(rOther.m_onDisplaced);
    rOther.m_pIndex = nullptr;
    rOther.m_pTile = nullptr;
    rOther.m_bUserAssigned = false;
    rOther.m_onDisplaced = nullptr;
    if (m_pIndex && m_pTile)
    {
        // Keep the index's back-pointer aimed at the claim's current address.
        m_pIndex->m_claims[m_pTile] = this;
    }
}

void WorkedTileClaim::Release_()
{
    if (m_pIndex && m_pTile)
    {
        m_pIndex->Release_(*m_pTile);
    }
    m_pIndex = nullptr;
    m_pTile = nullptr;
    m_bUserAssigned = false;
    m_onDisplaced = nullptr;
}

bool WorkedTileIndex::IsWorked(const Tile& rTile) const
{
    return m_claims.contains(&rTile);
}

WorkedTileClaim WorkedTileIndex::TryClaim(const Tile& rTile, bool bUserAssigned,
                                          DisplacedWorkerHandler onDisplaced)
{
    if (m_claims.contains(&rTile))
    {
        return WorkedTileClaim{};
    }
    m_revision.Bump();
    // The constructor registers the claim in m_claims.
    return WorkedTileClaim(*this, rTile, bUserAssigned, std::move(onDisplaced));
}

WorkedTileClaim WorkedTileIndex::ClaimDisplacing(const Tile& rTile, bool bUserAssigned)
{
    DisplacedWorkerHandler notifyDisplaced;
    auto it = m_claims.find(&rTile);
    if (it != m_claims.end())
    {
        WorkedTileClaim& rCurrent = *it->second;
        if (!rCurrent.m_onDisplaced)
        {
            throw std::runtime_error("WorkedTileIndex: tile (" + std::to_string(rTile.GetX())
                                     + ", " + std::to_string(rTile.GetY())
                                     + ") is a base's own tile and cannot be displaced");
        }
        notifyDisplaced = std::move(rCurrent.m_onDisplaced);
        rCurrent.Release_();
    }

    WorkedTileClaim claim = TryClaim(rTile, bUserAssigned);

    // Notify only once the new claim is registered, so the displaced worker's owner
    // re-assigns against a world in which this tile is already taken.
    if (notifyDisplaced)
    {
        notifyDisplaced();
    }
    return claim;
}

void WorkedTileIndex::Release_(const Tile& rTile)
{
    if (m_claims.erase(&rTile) == 0)
    {
        throw std::logic_error("WorkedTileIndex: released a tile that was not claimed");
    }
    m_revision.Bump();
}

} // namespace ac
