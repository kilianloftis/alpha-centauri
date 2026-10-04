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
// Moisture tier: inset only against drier land. Water/missing neighbors stay flush — insetting
// those edges would uniformly scale the diamond and pull land–land edges inward too.
SpriteEdgeMatch_t MatchMoistureTierEdges(const Tile& rTile, const WorldMap* pMap,
                                         Moisture_t minTier);
SpriteEdgeMatch_t MatchRockinessEdges(const Tile& rTile, const WorldMap* pMap);
SpriteEdgeMatch_t MatchFungusEdges(const Tile& rTile, const WorldMap* pMap);
// contentId is the landform layer id: "OceanShelf", "Ocean", or "water".
// Insets only against a different sea band; land neighbors stay flush (same scale trap as
// moisture vs water).
SpriteEdgeMatch_t MatchSeaLandformEdges(const Tile& rTile, const WorldMap* pMap,
                                        std::string_view contentId);

// Asymmetric AABB for a diamond-masked sprite. size is diamond width; height is size/2.
// Unmatched edges inset by insetRatio * size (mapped to AABB padding); matched edges stay flush.
SpriteDestRect_t DestRectForEdgeInsets(float x, float y, float size, const SpriteEdgeMatch_t& match,
                                       float insetRatio);

} // namespace ac
