#include "game/Faction.h"

#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/Economy.h"
#include "game/faction/Military.h"
#include "game/faction/Research.h"
#include "game/faction/Diplomacy.h"

namespace ac
{

Faction::Faction()
    : m_pIdentity(nullptr)
    , m_pAIProfile(nullptr)
    , m_pEconomy(nullptr)
    , m_pMilitary(nullptr)
    , m_pResearch(nullptr)
    , m_pDiplomacy(nullptr)
{
}

Faction::~Faction()
{
}

void Faction::ProcessTurn()
{
    // TODO: Delegate to subsystems
    // m_pEconomy->CalculateIncome();
    // m_pMilitary->UpdateUnits();
    // m_pResearch->AdvanceResearch();
    // m_pDiplomacy->UpdateRelations();
}

FactionIdentity* Faction::GetIdentity()
{
    return m_pIdentity.get();
}

const FactionIdentity* Faction::GetIdentity() const
{
    return m_pIdentity.get();
}

AIProfile* Faction::GetAIProfile()
{
    return m_pAIProfile.get();
}

const AIProfile* Faction::GetAIProfile() const
{
    return m_pAIProfile.get();
}

Economy* Faction::GetEconomy()
{
    return m_pEconomy.get();
}

const Economy* Faction::GetEconomy() const
{
    return m_pEconomy.get();
}

Military* Faction::GetMilitary()
{
    return m_pMilitary.get();
}

const Military* Faction::GetMilitary() const
{
    return m_pMilitary.get();
}

Research* Faction::GetResearch()
{
    return m_pResearch.get();
}

const Research* Faction::GetResearch() const
{
    return m_pResearch.get();
}

Diplomacy* Faction::GetDiplomacy()
{
    return m_pDiplomacy.get();
}

const Diplomacy* Faction::GetDiplomacy() const
{
    return m_pDiplomacy.get();
}

} // namespace ac
