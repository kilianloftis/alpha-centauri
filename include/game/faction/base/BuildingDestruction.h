#pragma once

#include "game/buildings/BuildingConfig.h"

#include <random>
#include <vector>

namespace ac
{

class BaseManager;
class GameState;

// Destroy one constructed building and tell the owning faction, so a cooling ASAT/interceptor
// deploy record for that copy is retired — every destruction path (raze, orbital attack,
// intercept fail, sabotage, riot) must notify or m_buildingDeploys goes stale and
// CountReadyBuildings under-counts the faction's remaining copies.
//
// A destroyed secret project is additionally tombstoned: removed from the world permanently,
// for every faction.
// TODO: that is consistent with raze / ASAT / intercept, but the SMAC rule for *sabotage*
// specifically is not recorded — one probe mission permanently deleting a secret project may
// not be right.
void DestroyBuildingAndNotify(GameState& rGameState, BaseManager& rBase,
                              const BuildingConfig_t& rBuilding);

// Facilities that may be randomly destroyed under the given filters.
std::vector<const BuildingConfig_t*> CollectDestroyableFacilities(const BaseManager& rBase,
                                                                  bool bExcludeHq,
                                                                  bool bExcludeSecretProjects);

// Destroy up to `count` uniformly shuffled eligible facilities. Returns the destroyed ids
// (empty when nothing was eligible).
std::vector<BuildingId_t> DestroyRandomFacilities(GameState& rGameState, BaseManager& rBase,
                                                  int count, bool bExcludeHq,
                                                  bool bExcludeSecretProjects, std::mt19937& rRng);

} // namespace ac
