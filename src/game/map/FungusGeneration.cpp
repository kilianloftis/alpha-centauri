#include "game/map/FungusGeneration.h"

#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

// Skew uniform[min, max] toward the small end: size = min + floor(span * u^skew).
// skew 1 → uniform; higher → more mass near min.
int SamplePatchSize_(int min, int max, float skew, std::mt19937& rRng)
{
    if (min >= max)
    {
        return min;
    }

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    const float u = unit(rRng);
    const float t = std::pow(u, skew);
    const int span = max - min + 1;
    const int size = min + static_cast<int>(t * static_cast<float>(span));
    return std::min(max, size);
}

bool IsEligible_(const Tile& rTile, bool bWantLand)
{
    if (rTile.GetHasFungus())
    {
        return false;
    }
    return bWantLand ? rTile.IsLand() : rTile.IsWater();
}

// True if rTile orthogonally touches fungus outside the current patch (would coalesce).
bool TouchesForeignFungus_(const Tile& rTile, WorldMap& rWorld,
                           const std::unordered_set<const Tile*>& rPatch)
{
    bool touches = false;
    ForEachOrthogonalNeighbor(rTile, rWorld, [&](const Tile* pNeighbor)
    {
        if (pNeighbor && pNeighbor->GetHasFungus() && rPatch.count(pNeighbor) == 0)
        {
            touches = true;
        }
    });
    return touches;
}

int GrowPatch_(WorldMap& rWorld, Tile& rSeed, int targetSize, bool bWantLand, std::mt19937& rRng)
{
    if (targetSize <= 0 || !IsEligible_(rSeed, bWantLand)
        || TouchesForeignFungus_(rSeed, rWorld, /*empty patch=*/{}))
    {
        return 0;
    }

    std::unordered_set<const Tile*> patch;
    rSeed.SetHasFungus(true);
    patch.insert(&rSeed);
    int placed = 1;
    if (placed >= targetSize)
    {
        return placed;
    }

    // At most one frontier slot per tile, however many patch members reach it, so every draw
    // is a distinct candidate.
    std::unordered_set<const Tile*> enqueued{&rSeed};
    std::vector<Tile*> frontier;
    auto enqueue = [&](Tile* pNeighbor) {
        if (IsEligible_(*pNeighbor, bWantLand) && !TouchesForeignFungus_(*pNeighbor, rWorld, patch)
            && enqueued.insert(pNeighbor).second)
        {
            frontier.push_back(pNeighbor);
        }
    };

    ForEachOrthogonalNeighbor(rSeed, rWorld, enqueue);

    while (placed < targetSize && !frontier.empty())
    {
        std::uniform_int_distribution<size_t> pick(0, frontier.size() - 1);
        const size_t index = pick(rRng);
        Tile* pNext = frontier[index];
        frontier[index] = frontier.back();
        frontier.pop_back();

        if (!pNext || !IsEligible_(*pNext, bWantLand)
            || TouchesForeignFungus_(*pNext, rWorld, patch))
        {
            continue;
        }

        pNext->SetHasFungus(true);
        patch.insert(pNext);
        ++placed;

        ForEachOrthogonalNeighbor(*pNext, rWorld, enqueue);
    }

    return placed;
}

void PlaceOnDomain_(WorldMap& rWorld,
                    float fraction,
                    int minPatch,
                    int maxPatch,
                    float sizeSkew,
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
        if (!pSeed || !IsEligible_(*pSeed, bWantLand)
            || TouchesForeignFungus_(*pSeed, rWorld, /*empty=*/{}))
        {
            continue;
        }

        const int desired = std::min(
            remaining, SamplePatchSize_(patchMin, patchMax, sizeSkew, rRng));
        const int grown = GrowPatch_(rWorld, *pSeed, desired, bWantLand, rRng);
        remaining -= grown;
    }
}

} // namespace

void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig, std::mt19937& rRng)
{
    const int minPatch = std::max(1, rConfig.minPatchTiles);
    const int maxPatch = std::max(minPatch, rConfig.maxPatchTiles);

    PlaceOnDomain_(rWorld, rConfig.landFraction, minPatch, maxPatch, rConfig.patchSizeSkew,
                   /*bWantLand=*/true, rRng);
    PlaceOnDomain_(rWorld, rConfig.waterFraction, minPatch, maxPatch, rConfig.patchSizeSkew,
                   /*bWantLand=*/false, rRng);
}

} // namespace ac
