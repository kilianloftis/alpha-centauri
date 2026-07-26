#include "game/map/FungusGeneration.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ac
{

namespace
{

int RandomIntInclusive_(int min, int max, std::mt19937& rRng)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(rRng);
}

bool IsEligible_(const Tile& rTile, bool bWantLand)
{
    if (rTile.GetHasFungus())
    {
        return false;
    }
    return bWantLand ? rTile.IsLand() : rTile.IsWater();
}

int GrowPatch_(WorldMap& rWorld, Tile& rSeed, int targetSize, bool bWantLand, std::mt19937& rRng)
{
    if (targetSize <= 0 || !IsEligible_(rSeed, bWantLand))
    {
        return 0;
    }

    rSeed.SetHasFungus(true);
    int placed = 1;
    if (placed >= targetSize)
    {
        return placed;
    }

    std::vector<Tile*> frontier;
    ForEachOrthogonalNeighbor(rSeed, rWorld, [&](Tile* pNeighbor)
    {
        if (IsEligible_(*pNeighbor, bWantLand))
        {
            frontier.push_back(pNeighbor);
        }
    });

    while (placed < targetSize && !frontier.empty())
    {
        std::uniform_int_distribution<size_t> pick(0, frontier.size() - 1);
        const size_t index = pick(rRng);
        Tile* pNext = frontier[index];
        frontier[index] = frontier.back();
        frontier.pop_back();

        if (!pNext || !IsEligible_(*pNext, bWantLand))
        {
            continue;
        }

        pNext->SetHasFungus(true);
        ++placed;

        ForEachOrthogonalNeighbor(*pNext, rWorld, [&](Tile* pNeighbor)
        {
            if (IsEligible_(*pNeighbor, bWantLand))
            {
                frontier.push_back(pNeighbor);
            }
        });
    }

    return placed;
}

void PlaceOnDomain_(WorldMap& rWorld,
                    float fraction,
                    int minPatch,
                    int maxPatch,
                    bool bWantLand,
                    std::mt19937& rRng)
{
    if (fraction <= 0.0f || maxPatch < 1)
    {
        return;
    }

    std::vector<Tile*> candidates;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && IsEligible_(*pTile, bWantLand))
        {
            candidates.push_back(pTile.get());
        }
    }
    if (candidates.empty())
    {
        return;
    }

    const int targetTotal = static_cast<int>(std::lround(
        fraction * static_cast<float>(candidates.size())));
    if (targetTotal <= 0)
    {
        return;
    }

    const int patchMin = std::max(1, minPatch);
    const int patchMax = std::max(patchMin, maxPatch);

    std::shuffle(candidates.begin(), candidates.end(), rRng);
    size_t candidateIndex = 0;
    int remaining = targetTotal;

    while (remaining > 0 && candidateIndex < candidates.size())
    {
        Tile* pSeed = candidates[candidateIndex++];
        if (!pSeed || !IsEligible_(*pSeed, bWantLand))
        {
            continue;
        }

        const int desired = std::min(remaining, RandomIntInclusive_(patchMin, patchMax, rRng));
        const int grown = GrowPatch_(rWorld, *pSeed, desired, bWantLand, rRng);
        remaining -= grown;
    }
}

} // namespace

void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig, std::mt19937& rRng)
{
    const int minPatch = std::max(1, rConfig.minPatchTiles);
    const int maxPatch = std::max(minPatch, rConfig.maxPatchTiles);

    PlaceOnDomain_(rWorld, rConfig.landFraction, minPatch, maxPatch, /*bWantLand=*/true, rRng);
    PlaceOnDomain_(rWorld, rConfig.waterFraction, minPatch, maxPatch, /*bWantLand=*/false, rRng);
}

} // namespace ac
