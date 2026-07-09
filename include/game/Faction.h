#pragma once

#include <memory>
#include <vector>

#include "game/IEffectsProvider.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/FactionConfig.h"
#include "game/faction/FactionEffectsPool.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "lib/DerefView.h"
#include "lib/Revision.h"
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
class FactionFlavor;
class EconomyManager;
class Military;
class ResearchManager;
class ResearchSelector;
class Diplomacy;
class SocialEngineeringManager;
class PopTypeAvailabilityCalculator;
class UnitManager;
struct PopTypeConfig_t;
struct GameDataContext;
class WorldMap;

class Faction : public IEffectsProvider
{
public:
    Faction(const FactionConfig_t& rDefinition,
             const BuildingRegistry* pBuildingRegistry,
             const TechRegistry* pTechRegistry,
             const SocialPolicyRegistry* pSocialPolicyRegistry,
             const SocialRatingRegistry* pSocialRatingRegistry,
             TechCostCalculator* pTechCostCalculator,
             const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator);
    ~Faction();

    const FactionConfig_t& GetDefinition() const { return m_rDefinition; }
    const std::string& GetDefinitionId() const { return m_rDefinition.id; }

    std::string SuggestBaseName();

    // Base management
    void AddBase(std::unique_ptr<BaseManager> pBase);
    BaseManager* CreateBase(FactionId factionId, int baseId, const std::string& name, Tile* pTile,
                            const GameDataContext& rDataContext,
                            TileEffectsContext& rTileEffects);
    // Iterate bases by reference without exposing the owning unique_ptrs.
    auto Bases() { return DerefView(m_bases); }
    auto Bases() const { return DerefView(m_bases); }
    size_t GetBaseCount() const { return m_bases.size(); }

    // Returns buildings the faction has the technology to build.
    std::vector<const BuildingConfig_t*> GetDiscoveredBuildings() const;

    // Economy subsystem: energy treasury and allocation split.
    EconomyManager& GetEconomy();
    const EconomyManager& GetEconomy() const;

    // Consume every base's accumulated econ stockpile into the treasury.
    // Returns the amount collected this call.
    int CollectIncome();

    // Consume every base's accumulated labs stockpile into research points.
    // Returns the amount collected this call.
    int CollectResearch();

    // Projected per-turn outputs summed across all bases.
    int GetNetIncomePerTurn() const;
    int GetBreakthroughRate() const;
    int GetTurnsUntilBreakthrough() const;

    // Resource production - routes to all bases and faction economy manager.
    // rExternalEffects are effects from outside this faction (other factions' WorldGlobal
    // effects, gathered by the turn stage via CollectWorldEffects); appended to the pool.
    void ProduceBaseResources(const std::vector<ActiveEffect_t>& rExternalEffects);

    // Apply growth to all bases, incorporating GrowthRate stat effects.
    void ApplyBaseGrowth(const std::vector<ActiveEffect_t>& rExternalEffects);

    // Research subsystem.
    ResearchManager& GetResearch();
    const ResearchManager& GetResearch() const;

    // Discover the current research target and auto-select the next one.
    bool DiscoverCurrentResearch();

    // Social engineering subsystem.
    SocialEngineeringManager& GetSocialEngineering();
    const SocialEngineeringManager& GetSocialEngineering() const;

    // Military (unit designs and units)
    Military& GetMilitary();
    const Military& GetMilitary() const;

    // Live units owned by this faction.
    UnitManager& GetUnitManager();
    const UnitManager& GetUnitManager() const;

    // Pop types
    std::vector<const PopTypeConfig_t*> GetAvailablePopTypes() const;

    // IEffectsProvider — delegates to the memoized FactionEffectsPool component.
    const FactionEffects_t& GetActiveEffects() const override;
    uint64_t GetEffectsVersion() const override;

private:
    int GetResearchPerTurn_() const;

    const FactionConfig_t& m_rDefinition;
    const BuildingRegistry* m_pBuildingRegistry;
    const PopTypeAvailabilityCalculator* m_pPopTypeAvailabilityCalculator;
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<FactionFlavor> m_pFlavor;
    std::unique_ptr<EconomyManager> m_pEconomy;
    std::unique_ptr<Military> m_pMilitary;
    std::unique_ptr<ResearchManager> m_pResearch;
    std::unique_ptr<ResearchSelector> m_pResearchSelector;
    std::unique_ptr<Diplomacy> m_pDiplomacy;
    std::unique_ptr<SocialEngineeringManager> m_pSocialEngineering;
    std::unique_ptr<UnitManager> m_pUnits;
    std::vector<std::unique_ptr<BaseManager>> m_bases;
    Revision m_baseListRevision; // bumped when a base is added (later: removed/captured)
    // Declared after m_baseListRevision, which its constructor binds a reference to.
    FactionEffectsPool m_effectsPool;
};

} // namespace ac
