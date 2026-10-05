#pragma once

#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <string_view>

namespace ac
{

// Ortho map neighbors mapped to isometric diamond edges (MapViewport AABB convention):
// N→NE, E→SE, S→SW, W→NW. True = matching neighbor → draw flush on that edge.
struct SpriteEdgeMatch_t
{
    bool bNe = false;
    bool bSe = false;
    bool bSw = false;
    bool bNw = false;
};

struct SpriteDestRect_t
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// null pMap → all edges mismatched (soft inset all around).
// Rockiness: inset against land with different rockiness. Water/missing neighbors stay flush —
// insetting those edges would uniformly scale the diamond and pull land–land edges inward too.
SpriteEdgeMatch_t MatchRockinessEdges(const Tile& rTile, const WorldMap* pMap);
// contentId is the landform layer id: "OceanShelf", "Ocean", or "water".
// Insets only against a different sea band; land neighbors stay flush (same scale trap).
SpriteEdgeMatch_t MatchSeaLandformEdges(const Tile& rTile, const WorldMap* pMap,
                                        std::string_view contentId);

// AABB of a scaled diamond-masked sprite inside the tile diamond. size is diamond width; height
// is size/2. Each unmatched edge moves in by insetRatio of the diamond's side; a matched edge
// stays flush unless its opposite edge also matches while the other pair insets, in which case
// the sprite centers between them. The sprite never extends past the tile.
SpriteDestRect_t DestRectForEdgeInsets(float x, float y, float size, const SpriteEdgeMatch_t& match,
                                       float insetRatio);

} // namespace ac
