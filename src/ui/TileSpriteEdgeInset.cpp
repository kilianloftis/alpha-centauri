#include "ui/TileSpriteEdgeInset.h"

#include "game/map/ImprovementIds.h"

#include <algorithm>
#include <cmath>

namespace ac
{

namespace
{

constexpr float k_IsoHeightRatio = 0.5f;

template<typename Pred>
SpriteEdgeMatch_t MatchOrthoEdges_(const Tile& rTile, const WorldMap* pMap, Pred&& matches)
{
    SpriteEdgeMatch_t match{};
    if (!pMap)
    {
        return match;
    }

    // Order N, E, S, W → diamond NE, SE, SW, NW.
    static constexpr int k_Deltas[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    bool* const edges[4] = {&match.bNe, &match.bSe, &match.bSw, &match.bNw};
    for (int i = 0; i < 4; ++i)
    {
        const Tile* pNeighbor =
            pMap->GetTile(rTile.GetX() + k_Deltas[i][0], rTile.GetY() + k_Deltas[i][1]);
        if (pNeighbor && matches(rTile, *pNeighbor))
        {
            *edges[i] = true;
        }
    }
    return match;
}

} // namespace

namespace
{

int MoistureTierRank_(Moisture_t moisture)
{
    switch (moisture)
    {
        case Moisture_t::Arid:
            return 0;
        case Moisture_t::Moist:
            return 1;
        case Moisture_t::Wet:
            return 2;
    }
    return 0;
}

} // namespace

SpriteEdgeMatch_t MatchMoistureTierEdges(const Tile& rTile, const WorldMap* pMap,
                                         Moisture_t minTier)
{
    (void)rTile;
    const int minRank = MoistureTierRank_(minTier);
    return MatchOrthoEdges_(rTile, pMap, [minRank](const Tile&, const Tile& rNeighbor) {
        // Sea is not a rainfall tier — keep flush so dest scaling does not nibble land seams.
        if (!rNeighbor.IsLand())
        {
            return true;
        }
        return MoistureTierRank_(rNeighbor.GetMoisture()) >= minRank;
    });
}

SpriteEdgeMatch_t MatchRockinessEdges(const Tile& rTile, const WorldMap* pMap)
{
    return MatchOrthoEdges_(rTile, pMap, [](const Tile& rSelf, const Tile& rNeighbor) {
        if (!rNeighbor.IsLand())
        {
            return true;
        }
        return rNeighbor.GetRockiness() == rSelf.GetRockiness();
    });
}

SpriteEdgeMatch_t MatchFungusEdges(const Tile& rTile, const WorldMap* pMap)
{
    return MatchOrthoEdges_(rTile, pMap, [](const Tile&, const Tile& rNeighbor) {
        if (!rNeighbor.IsLand())
        {
            return true;
        }
        return rNeighbor.HasFeature(ImprovementIds::k_Fungus);
    });
}

SpriteEdgeMatch_t MatchSeaLandformEdges(const Tile& rTile, const WorldMap* pMap,
                                        std::string_view contentId)
{
    return MatchOrthoEdges_(rTile, pMap, [contentId](const Tile&, const Tile& rNeighbor) {
        // Land is not a depth band — keep flush so dest scaling does not nibble sea–sea seams.
        if (!rNeighbor.IsWater())
        {
            return true;
        }
        if (contentId == "OceanShelf")
        {
            return rNeighbor.HasFeature("OceanShelf");
        }
        if (contentId == "Ocean")
        {
            return rNeighbor.HasFeature("Ocean");
        }
        return true;
    });
}

SpriteDestRect_t DestRectForEdgeInsets(float x, float y, float size, const SpriteEdgeMatch_t& match,
                                       float insetRatio)
{
    const float width = size;
    const float height = size * k_IsoHeightRatio;
    if (insetRatio <= 0.0f || !std::isfinite(insetRatio))
    {
        return SpriteDestRect_t{x, y, width, height};
    }

    const float insetW = insetRatio * size;
    const float insetH = insetW * k_IsoHeightRatio;

    float padL = 0.0f;
    float padR = 0.0f;
    float padT = 0.0f;
    float padB = 0.0f;
    if (!match.bNe)
    {
        padT = std::max(padT, insetH);
        padR = std::max(padR, insetW);
    }
    if (!match.bSe)
    {
        padB = std::max(padB, insetH);
        padR = std::max(padR, insetW);
    }
    if (!match.bSw)
    {
        padB = std::max(padB, insetH);
        padL = std::max(padL, insetW);
    }
    if (!match.bNw)
    {
        padT = std::max(padT, insetH);
        padL = std::max(padL, insetW);
    }

    const float left = x + padL;
    const float right = x + width - padR;
    const float top = y + padT;
    const float bottom = y + height - padB;
    const float availW = right - left;
    const float availH = bottom - top;
    if (availW <= 0.0f || availH <= 0.0f)
    {
        return SpriteDestRect_t{x, y, width, height};
    }

    // Keep a 2:1 diamond footprint inside the padded region, pinned flush to sides that
    // had no pad (matched neighbors). Centering here would pull coastal tiles away from
    // their land edges when only the water-facing sides inset.
    float destW = availW;
    float destH = destW * k_IsoHeightRatio;
    if (destH > availH)
    {
        destH = availH;
        destW = destH / k_IsoHeightRatio;
    }

    float destX = left;
    if (padL > 0.0f && padR == 0.0f)
    {
        destX = right - destW;
    }
    else if (padL > 0.0f && padR > 0.0f)
    {
        destX = left + (availW - destW) * 0.5f;
    }

    float destY = top;
    if (padT > 0.0f && padB == 0.0f)
    {
        destY = bottom - destH;
    }
    else if (padT > 0.0f && padB > 0.0f)
    {
        destY = top + (availH - destH) * 0.5f;
    }

    return SpriteDestRect_t{destX, destY, destW, destH};
}

} // namespace ac
