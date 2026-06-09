#pragma once

#include "game/map/Tile.h"
#include <vector>
#include <memory>

namespace ac
{

class WorldMap
{
public:
    WorldMap(int width, int height);
    ~WorldMap();

    // Map dimensions
    int GetWidth() const;
    int GetHeight() const;

    // Tile access
    Tile* GetTile(int x, int y);
    const Tile* GetTile(int x, int y) const;

    // Get all tiles
    std::vector<std::unique_ptr<Tile>>& GetTiles();
    const std::vector<std::unique_ptr<Tile>>& GetTiles() const;

private:
    int m_width;
    int m_height;
    std::vector<std::unique_ptr<Tile>> m_tiles;

    int GetTileIndex_(int x, int y) const;
};

} // namespace ac
