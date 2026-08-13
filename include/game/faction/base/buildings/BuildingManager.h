#pragma once

#include "game/buildings/BuildingConfig.h"
#include "lib/Revision.h"
#include "game/effects/ActiveEffect.h"
#include <string>
#include <vector>

namespace ac
{

class BaseManager;
class BuildingRegistry;
class ResearchManager;
class SecretProjectAvailabilityCalculator;

// BuildingManager tracks all buildings present in a single base.
// Buildings are added by id (looked up via BuildingRegistry) and destroyed by id.
// Provides aggregate bonus queries that sum across all held buildings.
class BuildingManager
{
public:
    // pSecretProjectCalculator is the one optional dependency (it reads live session state, so
    // it only exists inside a GameState); the others always come from the composition root.
    BuildingManager(const BuildingRegistry& rBuildingRegistry,
                    const SecretProjectAvailabilityCalculator* pSecretProjectCalculator,
                    const ResearchManager& rResearchManager);
    ~BuildingManager();

    // Whether buildingId may be added here: unknown ids, stockpile production items (never
    // constructed), and duplicates of a non-allowMultiple building are rejected, as is a
    // secret project already built or destroyed anywhere in the world. Throws only if the id
    // is unknown, or a secret project is queried without a
    // SecretProjectAvailabilityCalculator.
    bool CanAddBuilding(const BuildingId_t& buildingId) const;

    // Add a building by id. Throws unless CanAddBuilding holds — callers that can legitimately
    // lose a race (production completion) must check first.
    void AddBuilding(const BuildingId_t& buildingId);

    // Destroy the first building with the given id. No-op if not present.
    void DestroyBuilding(const BuildingId_t& buildingId);

    // Whether this base currently holds at least one copy.
    bool HasBuilding(const BuildingId_t& buildingId) const;

    // All currently constructed buildings.
    const std::vector<const BuildingConfig_t*>& GetBuildings() const;

    // Collect all continuous effects from constructed buildings in this base.
    // Passes rOriginBase into AppendActiveEffects so TagsOriginBase scopes are tagged.
    std::vector<ActiveEffect_t> CollectEffects(const BaseManager& rOriginBase) const;

    // Get a list of buildings that can be constructed at this base.
    // Discovered techs are read from the associated ResearchManager.
    // Base-local rules (allowMultiple, already built) are applied here.
    std::vector<const BuildingConfig_t*> GetBuildingsAvailableForConstruction() const;

    // Bumped on every building mutation; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

    // Ownership transfer (BaseManager::RebindFaction): tech-gated construction availability
    // must read the new owner's discovered techs from the next
    // GetBuildingsAvailableForConstruction call.
    void RebindResearch(const ResearchManager& rResearch);

private:
    bool DoesBuildingExist_(const BuildingId_t& buildingId) const;

    const BuildingRegistry& m_rRegistry;
    const ResearchManager* m_pResearch;
    const SecretProjectAvailabilityCalculator* m_pSecretProjectCalculator;
    std::vector<const BuildingConfig_t*> m_buildings;
    Revision m_revision;
};

} // namespace ac
