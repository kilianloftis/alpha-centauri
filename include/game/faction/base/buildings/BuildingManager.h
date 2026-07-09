#pragma once

#include "game/buildings/BuildingConfigParser.h"
#include "lib/Revision.h"
#include "lib/effects/ActiveEffect.h"
#include <string>
#include <vector>

namespace ac
{

struct GameDataContext;
class BuildingRegistry;
class ResearchManager;
class SecretProjectAvailabilityCalculator;

// BuildingManager tracks all buildings present in a single base.
// Buildings are added by id (looked up via BuildingRegistry) and destroyed by id.
// Provides aggregate bonus queries that sum across all held buildings.
class BuildingManager
{
public:
    BuildingManager(const GameDataContext& rDataContext,
                    const ResearchManager* pResearchManager);
    ~BuildingManager();

    // Add a building by id. Throws if the factory cannot find the id.
    void AddBuilding(const std::string& buildingId);

    // Destroy the first building with the given id. No-op if not present.
    void DestroyBuilding(const std::string& buildingId);

    // All currently constructed buildings.
    const std::vector<const BuildingConfig_t*>& GetBuildings() const;

    // Collect all continuous effects from constructed buildings in this base.
    // originBase is left nullptr; the caller (BaseManager) tags ThisBase-scoped effects.
    std::vector<ActiveEffect_t> CollectEffects() const;

    // Get a list of buildings that can be constructed at this base.
    // Discovered techs are read from the associated ResearchManager.
    // Base-local rules (allowMultiple, already built) are applied here.
    std::vector<const BuildingConfig_t*> GetBuildingsAvailableForConstruction() const;

    // Bumped on every building mutation; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    bool DoesBuildingExist_(const std::string& buildingId) const;

    const BuildingRegistry* m_pRegistry;
    const ResearchManager* m_pResearch;
    const SecretProjectAvailabilityCalculator* m_pSecretProjectCalculator;
    std::vector<const BuildingConfig_t*> m_buildings;
    Revision m_revision;
};

} // namespace ac
