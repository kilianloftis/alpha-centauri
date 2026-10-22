#pragma once

#include "game/buildings/BuildingConfig.h"

#include <random>
#include <vector>

namespace ac
{

class BaseManager;
class Faction;
class GameState;

struct OrbitalAttackResult_t
{
    bool bAttempted = false;
    bool bHit = false;
    bool bAttackerDestroyed = false;
    BuildingId_t attackerBuildingId;
    BuildingId_t targetBuildingId;
};

// Destroys one built building; OnBuildingDestroyed fans out to the deploy ledger and SP
// tombstones.
// TODO: whether sabotage should permanently delete a secret project is unconfirmed.
void DestroyBuildingAndNotify(BaseManager& rBase, const BuildingConfig_t& rBuilding);

// Destroy up to `count` uniformly shuffled eligible facilities. Returns the destroyed ids
// (empty when nothing was eligible).
std::vector<BuildingId_t> DestroyRandomFacilities(BaseManager& rBase, int count, bool bExcludeHq,
                                                  bool bExcludeSecretProjects, std::mt19937& rRng);

// ASAT with a chosen ready attacker building, which deploys either way. A hit destroys one
// defender orbital; a miss may lose the attacker. Throws for the same faction.
OrbitalAttackResult_t TryAttackSatellite(GameState& rGameState,
                                         Faction& rAttacker,
                                         Faction& rDefender,
                                         const BuildingId_t& attackerBuildingId,
                                         const BuildingId_t& targetOrbitalBuildingId,
                                         std::mt19937& rRng);

} // namespace ac
