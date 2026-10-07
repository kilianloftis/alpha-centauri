#include "game/map/LandmarkGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ac
{

namespace
{

bool DomainMatches_(const Tile& rTile, LandmarkDomain_t domain)
{
    switch (domain)
    {
    case LandmarkDomain_t::Land:
        return rTile.IsLand();
    case LandmarkDomain_t::Water:
        return rTile.IsWater();
    case LandmarkDomain_t::Any:
        return true;
    }
    return false;
}

std::vector<std::pair<int, int>> ExpandDisk_(int radius)
{
    std::vector<std::pair<int, int>> cells;
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            if (InEuclideanRadius(dx, dy, radius))
            {
                cells.emplace_back(dx, dy);
            }
        }
    }
    return cells;
}

std::vector<std::pair<int, int>> ExpandRing_(int outerRadius, int innerRadius)
{
    std::vector<std::pair<int, int>> cells;
    for (int dy = -outerRadius; dy <= outerRadius; ++dy)
    {
        for (int dx = -outerRadius; dx <= outerRadius; ++dx)
        {
            if (InEuclideanRadius(dx, dy, outerRadius)
                && !InEuclideanRadius(dx, dy, innerRadius))
            {
                cells.emplace_back(dx, dy);
            }
        }
    }
    return cells;
}

std::vector<std::pair<int, int>> ExpandMask_(const std::vector<std::string>& rRows)
{
    if (rRows.empty())
    {
        return {};
    }

    size_t width = 0;
    for (const std::string& row : rRows)
    {
        width = std::max(width, row.size());
    }
    const int originX = static_cast<int>(width / 2);
    const int originY = static_cast<int>(rRows.size() / 2);

    std::vector<std::pair<int, int>> cells;
    for (size_t y = 0; y < rRows.size(); ++y)
    {
        const std::string& row = rRows[y];
        for (size_t x = 0; x < row.size(); ++x)
        {
            const char c = row[x];
            if (c == 'X' || c == 'x')
            {
                cells.emplace_back(static_cast<int>(x) - originX, static_cast<int>(y) - originY);
            }
        }
    }
    return cells;
}

void ApplyRadialPeakSculpt_(WorldMap& rWorld, const Tile& rAnchor, int radius,
                            const LandmarkSculpt_t& rSculpt)
{
    const float rise = static_cast<float>(rSculpt.peakElevation - rSculpt.baseElevation);

    for (int q = -radius; q <= radius; ++q)
    {
        for (int p = -radius; p <= radius; ++p)
        {
            if (!InEuclideanRadius(p, q, radius))
            {
                continue;
            }
            Tile* pTile = GetTileAtLatticeOffset(rWorld, rAnchor, p, q);
            if (!pTile || !pTile->IsLand())
            {
                continue;
            }
            const float dist = std::sqrt(static_cast<float>(p * p + q * q));
            const float t = 1.0f - dist / static_cast<float>(radius + 1);
            const int elev = std::min(
                rSculpt.peakElevation,
                static_cast<int>(std::lround(static_cast<float>(rSculpt.baseElevation) + t * rise)));
            if (pTile->GetElevation() < elev)
            {
                pTile->SetElevation(elev);
            }
            if (dist < rSculpt.rockyCoreRadius)
            {
                pTile->SetRockiness(Rockiness_t::Rocky);
            }
            else if (pTile->GetRockiness() == Rockiness_t::Flat)
            {
                pTile->SetRockiness(Rockiness_t::Rolling);
            }
        }
    }
}

bool FarEnough_(const Tile& rCandidate, const std::vector<const Tile*>& rAnchors,
                int minSpacing, int mapWidth)
{
    for (const Tile* pAnchor : rAnchors)
    {
        if (ChebyshevDistance(rCandidate, *pAnchor, mapWidth) < minSpacing)
        {
            return false;
        }
    }
    return true;
}

bool TryStamp_(WorldMap& rWorld,
               int anchorX,
               int anchorY,
               const LandmarkConfig_t& rLandmark,
               const std::vector<std::pair<int, int>>& rOffsets,
               const ImprovementConfig_t& rOccupant,
               const ImprovementConfig_t* pFungus)
{
    std::vector<Tile*> footprint;
    footprint.reserve(rOffsets.size());

    const Tile* pAnchor = rWorld.GetTile(anchorX, anchorY);
    if (!pAnchor)
    {
        return false;
    }

    for (const auto& [p, q] : rOffsets)
    {
        Tile* pTile = GetTileAtLatticeOffset(rWorld, *pAnchor, p, q);
        if (!pTile || !DomainMatches_(*pTile, rLandmark.domain))
        {
            return false;
        }
        if (!CanBuildImprovement(*pTile, rOccupant))
        {
            return false;
        }
        footprint.push_back(pTile);
    }

    if (footprint.empty())
    {
        return false;
    }

    if (rLandmark.shape.kind == LandmarkShapeKind_t::Sculptor
        && rLandmark.shape.sculptorId == k_MountPlanetSculptor)
    {
        ApplyRadialPeakSculpt_(rWorld, *pAnchor, rLandmark.shape.radius,
                               rLandmark.shape.sculpt);
    }

    for (Tile* pTile : footprint)
    {
        pTile->AddOccupant(rOccupant);
        if (rLandmark.setFungus && pFungus)
        {
            pTile->AddTerrainFeature(*pFungus);
        }
    }
    return true;
}

} // namespace

std::vector<std::pair<int, int>> ExpandLandmarkShape(const LandmarkShape_t& rShape)
{
    switch (rShape.kind)
    {
    case LandmarkShapeKind_t::Disk:
        return ExpandDisk_(rShape.radius);
    case LandmarkShapeKind_t::Ring:
        return ExpandRing_(rShape.outerRadius, rShape.innerRadius);
    case LandmarkShapeKind_t::Mask:
        return ExpandMask_(rShape.maskRows);
    case LandmarkShapeKind_t::Sculptor:
        if (rShape.sculptorId == k_MountPlanetSculptor)
        {
            return ExpandDisk_(rShape.radius);
        }
        throw std::runtime_error("Unknown landmark sculptor '" + rShape.sculptorId + "'");
    }
    return {};
}

int PlaceLandmarks(WorldMap& rWorld,
                   const std::vector<LandmarkConfig_t>& rLandmarks,
                   const ImprovementRegistry& rOccupants,
                   std::mt19937& rRng)
{
    std::vector<const Tile*> placedAnchors;
    int placedCount = 0;

    for (const LandmarkConfig_t& rLandmark : rLandmarks)
    {
        if (rLandmark.maxCount <= 0)
        {
            continue;
        }

        const ImprovementConfig_t* pFungus =
            rOccupants.Find(std::string(ImprovementIds::k_Fungus));
        if (rLandmark.setFungus && !pFungus)
        {
            throw std::runtime_error("Landmark '" + rLandmark.id
                                     + "' sets fungus but there is no Fungus entry");
        }
        const ImprovementConfig_t* pOccupant = rOccupants.Find(rLandmark.improvementId);
        if (!pOccupant)
        {
            throw std::runtime_error(
                "Landmark '" + rLandmark.id + "' unknown occupant '"
                + rLandmark.improvementId + "'");
        }

        const std::vector<std::pair<int, int>> offsets = ExpandLandmarkShape(rLandmark.shape);
        if (offsets.empty())
        {
            throw std::runtime_error("Landmark '" + rLandmark.id
                                     + "' expands to an empty footprint and can never be placed");
        }

        std::vector<Tile*> candidates;
        for (auto& pTile : rWorld.GetTiles())
        {
            if (pTile && DomainMatches_(*pTile, rLandmark.domain))
            {
                candidates.push_back(pTile.get());
            }
        }
        std::shuffle(candidates.begin(), candidates.end(), rRng);

        int placedThis = 0;
        for (Tile* pAnchor : candidates)
        {
            if (placedThis >= rLandmark.maxCount)
            {
                break;
            }
            if (!FarEnough_(*pAnchor, placedAnchors, rLandmark.minSpacing, rWorld.GetWidth()))
            {
                continue;
            }
            if (TryStamp_(rWorld, pAnchor->GetX(), pAnchor->GetY(), rLandmark, offsets,
                          *pOccupant, pFungus))
            {
                placedAnchors.push_back(pAnchor);
                ++placedThis;
                ++placedCount;
            }
        }
    }

    return placedCount;
}

} // namespace ac
