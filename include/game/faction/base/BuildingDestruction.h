#pragma once

#include "game/buildings/BuildingConfig.h"

#include <random>
#include <vector>

namespace ac
{

class BaseManager;

// Destroy one constructed building. BuildingManager::OnBuildingDestroyed fans out to the
// owning faction (deploy ledger) and secret-project tombstone subscribers.
//
// TODO: that is consistent with raze / ASAT / intercept, but the SMAC rule for *sabotage*
// specifically is not recorded — one probe mission permanently deleting a secret project may
// not be right.
void DestroyBuildingAndNotify(BaseManager& rBase, const BuildingConfig_t& rBuilding);

// Facilities that may be randomly destroyed under the given filters.
std::vector<const BuildingConfig_t*> CollectDestroyableFacilities(const BaseManager& rBase,
                                                                  bool bExcludeHq,
                                                                  bool bExcludeSecretProjects);

// Destroy up to `count` uniformly shuffled eligible facilities. Returns the destroyed ids
// (empty when nothing was eligible).
std::vector<BuildingId_t> DestroyRandomFacilities(BaseManager& rBase, int count, bool bExcludeHq,
                                                  bool bExcludeSecretProjects, std::mt19937& rRng);

} // namespace ac
