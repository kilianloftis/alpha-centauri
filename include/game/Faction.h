#pragma once

#include <memory>

namespace ac
{

// Forward declarations
class FactionIdentity;
class AIProfile;
class Economy;
class Military;
class Research;
class Diplomacy;

class Faction
{
public:
    Faction();
    ~Faction();

    // Turn processing
    void ProcessTurn();

    // Subsystem accessors
    FactionIdentity* GetIdentity();
    const FactionIdentity* GetIdentity() const;
    
    AIProfile* GetAIProfile();
    const AIProfile* GetAIProfile() const;
    
    Economy* GetEconomy();
    const Economy* GetEconomy() const;
    
    Military* GetMilitary();
    const Military* GetMilitary() const;
    
    Research* GetResearch();
    const Research* GetResearch() const;
    
    Diplomacy* GetDiplomacy();
    const Diplomacy* GetDiplomacy() const;

private:
    std::unique_ptr<FactionIdentity> m_pIdentity;
    std::unique_ptr<AIProfile> m_pAIProfile;
    std::unique_ptr<Economy> m_pEconomy;
    std::unique_ptr<Military> m_pMilitary;
    std::unique_ptr<Research> m_pResearch;
    std::unique_ptr<Diplomacy> m_pDiplomacy;
};

} // namespace ac
