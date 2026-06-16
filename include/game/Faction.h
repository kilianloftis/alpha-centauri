#pragma once

#include <memory>
#include <vector>

#include "game/faction/base/BaseManager.h"
#include "game/social-engineering/SocialPolicyConfig.h"

namespace ac
{

// Forward declarations
class TechRegistry;
class SocialPolicyRegistry;
class TechCostCalculator;
class FactionIdentity;
class AIProfile;
class BaseEconomyManager;
class Military;
class ResearchManager;
class Diplomacy;
class SocialEngineeringManager;
struct GameDataContext;
class WorldMap;

class Faction
{
public:
    Faction(const TechRegistry* pTechRegistry, const SocialPolicyRegistry* pSocialPolicyRegistry,
             TechCostCalculator* pTechCostCalculator);
    ~Faction();

    // Turn processing
    void ProcessTurn();

    // Base management
    void AddBase(std::unique_ptr<BaseManager> pBase);
    BaseManager* CreateBase(FactionId factionId, int baseId, const std::string& name, int x, int y,
                            const GameDataContext& rDataContext,
                            const WorldMap& rWorldMap);
    BaseManager* GetBase(size_t index);
    const BaseManager* GetBase(size_t index) const;
    size_t GetBaseCount() const { return m_bases.size(); }

    // Energy tracking
    void AddEnergy(int amount);
    int GetEnergy() const;

    // Resource collection - routes to all bases and faction economy manager
    void CollectBaseResources();

    // Research - delegated to ResearchManager
    void AddResearchPoints(int points);
    int GetResearchPoints() const;

    // Social engineering
    bool SetSocialPolicy(SocialCategory category, const std::string& policyId);
    const SocialPolicyConfig* GetSocialPolicy(SocialCategory category) const;
    SocialScores GetSocialScores() const;
    std::vector<const SocialPolicyConfig*> GetAvailableSocialPolicies(
        SocialCategory category,
        const std::vector<std::string>& rDiscoveredTechIds) const;

private:
    int m_energy = 0;
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<BaseEconomyManager> m_pEconomy;
    std::unique_ptr<Military> m_pMilitary;
    std::unique_ptr<ResearchManager> m_pResearch;
    std::unique_ptr<Diplomacy> m_pDiplomacy;
    std::unique_ptr<SocialEngineeringManager> m_pSocialEngineering;
    std::vector<std::shared_ptr<BaseManager>> m_bases;
};

} // namespace ac
