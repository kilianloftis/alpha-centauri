#pragma once

#include "game/buildings/BuildingConfig.h"
#include "lib/Revision.h"
#include "lib/Signal.h"
#include "game/effects/ActiveEffect.h"
#include <string>
#include <vector>

namespace ac
{

class BaseManager;
class BuildingRegistry;
class ResearchManager;

// The buildings in one base.
class BuildingManager
{
public:
    BuildingManager(const BuildingRegistry& rBuildingRegistry,
                    const ResearchManager& rResearchManager);
    ~BuildingManager();

    // Rejects unknown ids, duplicates of a single-copy building, and secret projects built or
    // destroyed anywhere. Throws for an unknown id.
    bool CanAddBuilding(const BuildingId_t& buildingId) const;

    // Add a building by id. Throws unless CanAddBuilding holds — callers that can legitimately
    // lose a race (production completion) must check first.
    void AddBuilding(const BuildingId_t& buildingId);

    // Destroys the first copy, then emits OnBuildingDestroyed. No-op if absent.
    void DestroyBuilding(const BuildingId_t& buildingId);

    // Fired after DestroyBuilding removes a constructed copy (config pointer is registry-owned).
    Signal<const BuildingConfig_t&> OnBuildingDestroyed;

    // Whether this base currently holds at least one copy.
    bool HasBuilding(const BuildingId_t& buildingId) const;

    // The constructed copy's config, or null when this base does not hold one. Targeted
    // sabotage starts from an id but needs the config to decide secret-project tombstoning.
    const BuildingConfig_t* FindBuilding(const BuildingId_t& buildingId) const;

    // All currently constructed buildings.
    const std::vector<const BuildingConfig_t*>& GetBuildings() const;

    // Collect all continuous effects from constructed buildings in this base.
    // Passes rOriginBase into AppendActiveEffects so TagsOriginBase scopes are tagged.
    std::vector<ActiveEffect_t> CollectEffects(const BaseManager& rOriginBase) const;

    // Buildings this base can construct now, by tech and base-local rules.
    std::vector<const BuildingConfig_t*> GetBuildingsAvailableForConstruction() const;

    // Bumped on every building mutation; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

    // For ownership transfer: availability reads the new owner's techs from now on.
    void RebindResearch(const ResearchManager& rResearch);

private:
    bool DoesBuildingExist_(const BuildingId_t& buildingId) const;

    const BuildingRegistry& m_rRegistry;
    const ResearchManager* m_pResearch;
    std::vector<const BuildingConfig_t*> m_buildings;
    Revision m_revision;
};

} // namespace ac
