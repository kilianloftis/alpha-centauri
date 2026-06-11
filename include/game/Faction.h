#pragma once

#include <memory>
#include <vector>

#include "game/faction/base/BaseManager.h"

namespace ac
{

// Forward declarations
class FactionIdentity;
class AIProfile;
class BaseEconomyManager;
class Military;
class ResearchManager;
class Diplomacy;

class Faction
{
public:
    Faction();
    ~Faction();

    // Turn processing
    void ProcessTurn();

    // Base management
    void AddBase(std::unique_ptr<BaseManager> pBase);
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

private:
    int m_energy = 0;
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<BaseEconomyManager> m_pEconomy;
    std::unique_ptr<Military> m_pMilitary;
    std::unique_ptr<ResearchManager> m_pResearch;
    std::unique_ptr<Diplomacy> m_pDiplomacy;
    std::vector<std::shared_ptr<BaseManager>> m_bases;
};

} // namespace ac
