#pragma once

#include "game/natives/NativeUnitConfig.h"

#include <vector>

namespace ac
{

class Faction;
class PlanetFaunaCalculator;
class Tile;
class WorldMap;

// True when any faction's active effects resolve SuppressPlanetFauna (owned building pool or
// destroyed SP WorldGlobal, peer copies, and other sources).
bool PlanetFaunaSuppressed();

// Planet fauna formula helpers. Amounts come from PlanetFaunaCalculator.
bool PlanetFaunaEarlyLimit(int turnNumber, const PlanetFaunaCalculator& rCalc, int faunaForms,
                           int faunaDiff);
// cargoScale is resolved planet_fauna_cargo_scale (see PlanetFaunaCargoScale).
int PlanetFaunaBoatCargo(int turnNumber, const PlanetFaunaCalculator& rCalc, int faunaForms,
                         int faunaDiff, double cargoScale, bool bEarlyLimit);
double ResolvePlanetFaunaCargoScale(const Faction& rFaction);
bool PlanetFaunaSwarmYearReached(int turnNumber, const PlanetFaunaCalculator& rCalc,
                                 int faunaForms);

// Boat site: sea fungus (shelf) or empty ocean with a large enough region. Not land fungus.
bool AcceptsBoatSpawnSite(const Tile& rTile, const WorldMap& rMap);

// Candidates for a Planet fauna role that match tech gates. When pDomainTile is set, also
// require CanEnterTileTerrain; pass null for boat cargo (land passengers embark on water).
std::vector<const NativeUnitConfig_t*> FaunaCandidatesForRole(NativeFaunaRole_t role,
                                                              const Tile* pDomainTile);

} // namespace ac
