#pragma once

#include <memory>
#include <vector>

#include "game/faction/base/BaseManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "lib/effects/ActiveEffect.h"

namespace ac
{

// Forward declarations
class BuildingRegistry;
class TechRegistry;
class SocialPolicyRegistry;
class SocialRatingRegistry;
class TechCostCalculator;
class FactionIdentity;
class AIProfile;
class EconomyManager;
class Military;
class ResearchManager;
class Diplomacy;
class SocialEngineeringManager;
class PopTypeAvailabilityCalculator;
class UnitManager;
struct PopTypeConfig_t;
struct GameDataContext;
class WorldMap;

class Faction
{
public:
    Faction(const BuildingRegistry* pBuildingRegistry, const TechRegistry* pTechRegistry,
             const SocialPolicyRegistry* pSocialPolicyRegistry,
             const SocialRatingRegistry* pSocialRatingRegistry,
             TechCostCalculator* pTechCostCalculator,
             const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator);
    ~Faction();

    // Base management
    void AddBase(std::unique_ptr<BaseManager> pBase);
    BaseManager* CreateBase(FactionId factionId, int baseId, const std::string& name, Tile* pTile,
                            const GameDataContext& rDataContext,
                            TileEffectsContext& rTileEffects);
    BaseManager* GetBase(size_t index);
    const BaseManager* GetBase(size_t index) const;
    const std::vector<std::unique_ptr<BaseManager>>& GetBases() const { return m_bases; }
    size_t GetBaseCount() const { return m_bases.size(); }

    // Returns buildings the faction has the technology to build.
    std::vector<const BuildingConfig_t*> GetDiscoveredBuildings() const;

    // Energy tracking
    void AddEnergy(int amount);
    int GetEnergy() const;

    // Economy manager owns the faction-wide energy allocation split.
    EconomyManager* GetEconomyManager() const;

    // Resource production - routes to all bases and faction economy manager.
    // rExternalEffects are effects from outside this faction (other factions' WorldGlobal
    // effects, gathered by the turn stage via CollectWorldEffects); appended to the pool.
    void ProduceBaseResources(const std::vector<ActiveEffect_t>& rExternalEffects);

    // Apply growth to all bases, incorporating GrowthRate stat effects.
    void ApplyBaseGrowth(const std::vector<ActiveEffect_t>& rExternalEffects);

    // Research - delegated to ResearchManager
    void AddResearchPoints(int points);
    int GetResearchPoints() const;
    ResearchManager* GetResearchManager() const;

    // Building effects
    std::vector<ActiveEffect_t> CollectBuildingEffects() const;

    // Faction-lane effects (any scope except the locally-resolved ThisPop/ThisBase) declared
    // by pops living in this faction's bases. ThisBase pop effects merge per base via
    // CollectFromPops; ThisPop effects are resolved by the pop itself.
    std::vector<ActiveEffect_t> CollectPopFactionEffects() const;

    // Faction-lane effects (any scope except the locally-resolved ThisUnit/ThisTile) declared
    // by the components of this faction's live units — e.g. a component that generates +1
    // faction-wide energy while a unit carrying it exists.
    std::vector<ActiveEffect_t> CollectUnitFactionEffects() const;

    // Social engineering
    bool SetSocialPolicy(SocialCategory category, const std::string& policyId);
    const SocialPolicyConfig* GetSocialPolicy(SocialCategory category) const;
    std::vector<ActiveEffect_t> CollectSocialEffects() const;
    std::vector<const SocialPolicyConfig*> GetAvailableSocialPolicies(
        SocialCategory category,
        const std::vector<std::string>& rDiscoveredTechIds) const;

    // Military (unit designs and units)
    Military& GetMilitary();
    const Military& GetMilitary() const;

    // Live units owned by this faction.
    UnitManager& GetUnitManager();
    const UnitManager& GetUnitManager() const;

    // Pop types
    std::vector<const PopTypeConfig_t*> GetAvailablePopTypes() const;

private:
    int m_energy = 0;
    const BuildingRegistry* m_pBuildingRegistry;
    const PopTypeAvailabilityCalculator* m_pPopTypeAvailabilityCalculator;
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<EconomyManager> m_pEconomy;
    std::unique_ptr<Military> m_pMilitary;
    std::unique_ptr<ResearchManager> m_pResearch;
    std::unique_ptr<Diplomacy> m_pDiplomacy;
    std::unique_ptr<SocialEngineeringManager> m_pSocialEngineering;
    std::unique_ptr<UnitManager> m_pUnits;
    std::vector<std::unique_ptr<BaseManager>> m_bases;
};

} // namespace ac
