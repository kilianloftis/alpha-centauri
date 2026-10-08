#pragma once

#include "graphics/Graphics.h"
#include "ui/UIElement.h"

#include <cstddef>

namespace ac
{

class Unit;

// Unit chips: drawn on the map by MapRenderer and in the dashboard and base panels.
class UnitMarkerRenderer
{
public:
    // The marker in slot `slot` (0 first) on a tile whose footprint starts at (tileX, tileY):
    // slots run right from the tile's centre, spaced as the map lays out a tile's units.
    static Rectangle_t MarkerRectOnTile(float tileX, float tileY, float tileWidth,
                                        std::size_t slot);

    // Draw a single unit chip into rMarker.
    static void DrawMarker(Graphics& rGraphics, const Unit& rUnit, const Rectangle_t& rMarker);

    // The selection border around a drawn marker.
    static void DrawSelection(Graphics& rGraphics, const Rectangle_t& rMarker);

    // Placeholder hit graphic over a marker. Replace with a hit animation later.
    static void DrawHitOverlay(Graphics& rGraphics, const Rectangle_t& rMarker);
};

} // namespace ac
