#pragma once

#include <string_view>

namespace ac
{

class ImprovementRegistry;

// Terrain a tile derives from other state (the two axes and the depth bands); nothing may add
// or clear it.
bool IsDerivedTerrainId(std::string_view id);

// Throws unless every terrain enumerator and Fungus is a terrain occupant, and Forest and
// KelpFarm are improvements. Call once after loading.
void ValidateTerrainFeatures(const ImprovementRegistry& rOccupants);

} // namespace ac
