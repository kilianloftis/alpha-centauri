#include "ui/TileSpriteEdgeInset.h"

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

SpriteDestRect_t DestRectForEdgeInsets(float x, float y, float size, const SpriteEdgeMatch_t& match,
                                       float insetRatio)
{
    const float width = size;
    const float height = size * k_IsoHeightRatio;
    if (insetRatio <= 0.0f || !std::isfinite(insetRatio))
    {
        return SpriteDestRect_t{x, y, width, height};
    }

    // Fit the scaled diamond in the tile's own unit square: s runs from the W corner toward N
    // (SW edge at s = 0, NE edge at s = 1), t from W toward S (NW edge at t = 0, SE edge at
    // t = 1). Staying inside that square keeps the sprite inside the tile diamond.
    const float padSw = match.bSw ? 0.0f : insetRatio;
    const float padNe = match.bNe ? 0.0f : insetRatio;
    const float padNw = match.bNw ? 0.0f : insetRatio;
    const float padSe = match.bSe ? 0.0f : insetRatio;
    const float scale = std::min(1.0f - padSw - padNe, 1.0f - padNw - padSe);
    if (scale <= 0.0f)
    {
        return SpriteDestRect_t{x, y, width, height};
    }

    // Flush against the matched side when only the other side insets; otherwise centered.
    const auto offset = [scale](float padLow, float padHigh) {
        if (padLow == 0.0f && padHigh > 0.0f)
        {
            return 0.0f;
        }
        if (padHigh == 0.0f && padLow > 0.0f)
        {
            return 1.0f - scale;
        }
        return padLow + (1.0f - padLow - padHigh - scale) * 0.5f;
    };
    const float s0 = offset(padSw, padNe);
    const float t0 = offset(padNw, padSe);

    // The W corner (s0, t0) is the AABB's left edge; the N corner (s0 + scale, t0) its top.
    return SpriteDestRect_t{x + width * (s0 + t0) * 0.5f,
                            y + height * ((t0 - s0 - scale) * 0.5f + 0.5f), width * scale,
                            height * scale};
}

} // namespace ac
