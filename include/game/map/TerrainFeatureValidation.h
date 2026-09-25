#pragma once

#include <string_view>

namespace ac
{

class ImprovementRegistry;

// True for terrain ids a tile derives from other state rather than storing in its own right:
// the two axes (Rockiness_t / Moisture_t, always exactly one each) and the elevation depth
// bands. Nothing may add or clear these as occupants.
bool IsDerivedTerrainId(std::string_view id);

// Throws unless every Rockiness_t, Moisture_t and TerrainFeature_t enumerator, plus Fungus,
// is a terrain occupant, and Forest and KelpFarm are buildable improvements. Tile mirrors the
// enums into GetTerrainFeatures() by name, so a missing entry would otherwise cost a tile its
// terrain effects with no diagnostic. Call once after the registry is loaded, before any Tile
// is bound to it.
void ValidateTerrainFeatures(const ImprovementRegistry& rOccupants);

} // namespace ac
