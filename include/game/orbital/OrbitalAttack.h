#pragma once

#include "game/buildings/BuildingConfigParser.h"

#include <random>
#include <vector>

namespace ac
{

class Faction;
class GameState;

struct OrbitalAttackResult_t
{
    bool bAttempted = false;
    bool bHit = false;
    BuildingId_t attackerBuildingId;
    BuildingId_t targetBuildingId;
};

// One ready ASAT source the UI can present. One entry per building id that currently has
// at least one undeployed copy with an OrbitalAttack effect.
struct OrbitalAttackerOption_t
{
    BuildingId_t buildingId;
    // Non-owning pointer into the building registry / constructed config.
    const BuildingConfig_t* pConfig = nullptr;
    int readyCount = 0;
    // From the building's OrbitalAttack effect (first such effect on the config).
    int chance = 0;
    int cooldownTurns = 0;
};

// Ready OrbitalAttack buildings for rFaction at missionYear (empty when none can fire).
std::vector<OrbitalAttackerOption_t> ListReadyOrbitalAttackers(const Faction& rFaction,
                                                               int missionYear);

// Player/AI-initiated ASAT using a chosen ready attacker building. Deploys that building
// (hit or miss). On hit, destroys one instance of an orbital building on the defender.
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
