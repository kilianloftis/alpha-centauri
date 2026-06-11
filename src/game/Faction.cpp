#include "game/Faction.h"

#include "game/faction/FactionIdentity.h"
#include "game/faction/AIProfile.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/Military.h"
#include "game/faction/Research.h"
#include "game/faction/Diplomacy.h"

namespace ac
{

Faction::Faction()
    : m_pIdentity(nullptr)
    , m_pAIProfile(nullptr)
    , m_pEconomy(std::make_unique<BaseEconomyManager>())
    , m_pMilitary(nullptr)
    , m_pResearch(std::make_unique<Research>())
    , m_pDiplomacy(nullptr)
{
}

Faction::~Faction()
{
}

void Faction::AddEnergy(int amount)
{
    m_energy += amount;
}

int Faction::GetEnergy() const
{
    return m_energy;
}

void Faction::ProcessTurn()
{
    // TODO: Delegate to subsystems
    // m_pEconomy->CalculateIncome();
    // m_pMilitary->UpdateUnits();
    // m_pResearch->AdvanceResearch();
    // m_pDiplomacy->UpdateRelations();
}

void Faction::AddBase(std::unique_ptr<BaseManager> pBase)
{
    if (pBase)
    {
        m_bases.push_back(std::move(pBase));
    }
}

BaseManager* Faction::GetBase(size_t index)
{
    if (index < m_bases.size())
    {
        return m_bases[index].get();
    }
    return nullptr;
}

const BaseManager* Faction::GetBase(size_t index) const
{
    if (index < m_bases.size())
    {
        return m_bases[index].get();
    }
    return nullptr;
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

BaseEconomyManager* Faction::GetEconomy()
{
    return m_pEconomy.get();
}

const BaseEconomyManager* Faction::GetEconomy() const
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
