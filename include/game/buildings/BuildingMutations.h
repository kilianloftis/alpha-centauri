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

// Destroy one constructed building. BuildingManager::OnBuildingDestroyed fans out to the
// owning faction (deploy ledger) and secret-project tombstone subscribers.
//
// TODO: that is consistent with raze / ASAT / intercept, but the SMAC rule for *sabotage*
// specifically is not recorded — one probe mission permanently deleting a secret project may
// not be right.
void DestroyBuildingAndNotify(BaseManager& rBase, const BuildingConfig_t& rBuilding);

// Destroy up to `count` uniformly shuffled eligible facilities. Returns the destroyed ids
// (empty when nothing was eligible).
std::vector<BuildingId_t> DestroyRandomFacilities(BaseManager& rBase, int count, bool bExcludeHq,
                                                  bool bExcludeSecretProjects, std::mt19937& rRng);

// Player/AI-initiated ASAT using a chosen ready attacker building. Deploys that building
// (hit or miss). On hit, destroys one instance of an orbital building on the defender.
// On miss, may destroy the attacker per chanceOfDestructionOnFail.
// bAttempted == false when the target is missing/non-orbital, or attackerBuildingId is not a
// ready OrbitalAttack source for rAttacker.
// Throws std::logic_error when rAttacker and rDefender are the same faction.
OrbitalAttackResult_t TryAttackSatellite(GameState& rGameState,
                                         Faction& rAttacker,
                                         Faction& rDefender,
                                         const BuildingId_t& attackerBuildingId,
                                         const BuildingId_t& targetOrbitalBuildingId,
                                         std::mt19937& rRng);

} // namespace ac
