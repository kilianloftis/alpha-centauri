#pragma once

namespace ac
{

class ImprovementRegistry;

// Throws if improvements.json is missing an entry for any Rockiness_t, Moisture_t or
// TerrainFeature_t enumerator. Tile mirrors those enums into GetTerrainFeatures() by name, so
// a missing entry would otherwise cost a tile its terrain effects with no diagnostic. Call
// once after the improvement registry is loaded, before any Tile is bound to it.
void ValidateTerrainFeatures(const ImprovementRegistry& rImprovements);

} // namespace ac
