#pragma once

#include "game/buildings/BuildingConfig.h"

#include <vector>

namespace ac
{

class BaseManager;
class Faction;
struct OrbitalAttackEffect_t;

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

// Facilities that may be randomly destroyed under the given filters.
std::vector<const BuildingConfig_t*> CollectDestroyableFacilities(const BaseManager& rBase,
                                                                  bool bExcludeHq,
                                                                  bool bExcludeSecretProjects);

// The building's first OrbitalAttack effect, or null when it cannot attack satellites.
const OrbitalAttackEffect_t* FindOrbitalAttackEffect(const BuildingConfig_t& rBuilding);

// Ready OrbitalAttack buildings for rFaction at missionYear (empty when none can fire).
std::vector<OrbitalAttackerOption_t> ListReadyOrbitalAttackers(const Faction& rFaction,
                                                               int missionYear);

} // namespace ac
