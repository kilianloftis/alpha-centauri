#include "game/map/FungusGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <string_view>
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

bool IsEligible_(const Tile& rTile, const ImprovementConfig_t& rFungus)
{
    if (rTile.HasTerrainFeature(rFungus.id))
    {
        return false;
    }
    return CanBuildImprovement(rTile, rFungus);
}

// True if rTile orthogonally touches fungus outside the current patch (would coalesce).
bool TouchesForeignFungus_(const Tile& rTile, WorldMap& rWorld,
                           const std::unordered_set<const Tile*>& rPatch,
                           std::string_view fungusId)
{
    bool touches = false;
    ForEachOrthogonalNeighbor(rTile, rWorld, [&](const Tile* pNeighbor)
    {
        if (pNeighbor && pNeighbor->HasTerrainFeature(fungusId) && rPatch.count(pNeighbor) == 0)
        {
            touches = true;
        }
    });
    return touches;
}

int GrowPatch_(WorldMap& rWorld, Tile& rSeed, int targetSize,
               const ImprovementConfig_t& rFungus, std::mt19937& rRng)
{
    if (targetSize <= 0 || !IsEligible_(rSeed, rFungus)
        || TouchesForeignFungus_(rSeed, rWorld, /*empty patch=*/{}, rFungus.id))
    {
        return 0;
    }

    std::unordered_set<const Tile*> patch;
    rSeed.AddTerrainFeature(rFungus);
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
        if (IsEligible_(*pNeighbor, rFungus)
            && !TouchesForeignFungus_(*pNeighbor, rWorld, patch, rFungus.id)
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

        if (!pNext || !IsEligible_(*pNext, rFungus)
            || TouchesForeignFungus_(*pNext, rWorld, patch, rFungus.id))
        {
            continue;
        }

        pNext->AddTerrainFeature(rFungus);
        patch.insert(pNext);
        ++placed;

        ForEachOrthogonalNeighbor(*pNext, rWorld, enqueue);
    }

    return placed;
}

void PlacePatches_(WorldMap& rWorld,
                   float fraction,
                   int minPatch,
                   int maxPatch,
                   float sizeSkew,
                   const ImprovementConfig_t& rFungus,
                   std::mt19937& rRng)
{
    if (fraction <= 0.0f || maxPatch < 1)
    {
        return;
    }

    std::vector<Tile*> candidates;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && IsEligible_(*pTile, rFungus))
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
        if (!pSeed || !IsEligible_(*pSeed, rFungus)
            || TouchesForeignFungus_(*pSeed, rWorld, /*empty=*/{}, rFungus.id))
        {
            continue;
        }

        const int desired = std::min(
            remaining, SamplePatchSize_(patchMin, patchMax, sizeSkew, rRng));
        const int grown = GrowPatch_(rWorld, *pSeed, desired, rFungus, rRng);
        remaining -= grown;
    }
}

} // namespace

void PlaceFungus(WorldMap& rWorld, const FungusDecorationConfig_t& rConfig,
                 const ImprovementConfig_t& rFungus, std::mt19937& rRng)
{
    const int minPatch = std::max(1, rConfig.minPatchTiles);
    const int maxPatch = std::max(minPatch, rConfig.maxPatchTiles);

    PlacePatches_(rWorld, rConfig.fraction, minPatch, maxPatch, rConfig.patchSizeSkew, rFungus,
                  rRng);
}

} // namespace ac
