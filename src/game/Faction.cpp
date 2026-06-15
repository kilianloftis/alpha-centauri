#include "game/Faction.h"

#include "game/GameDataContext.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/faction/FactionIdentity.h"
#include <iostream>
#include "game/faction/AIProfile.h"
#include "game/faction/base/resources/BaseEconomyManager.h"
#include "game/faction/Military.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/Diplomacy.h"

namespace ac
{

Faction::Faction(const TechRegistry* pTechRegistry)
    : m_pIdentity(nullptr)
    , m_pAIProfile(nullptr)
    , m_pEconomy(std::make_unique<BaseEconomyManager>())
    , m_pMilitary(nullptr)
    , m_pResearch(std::make_unique<ResearchManager>(pTechRegistry))
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

BaseManager* Faction::CreateBase(FactionId factionId, int baseId, const std::string& name, int x, int y,
                                  const GameDataContext& rDataContext,
                                  std::function<const Tile*(int tileX, int tileY)> tileLookup)
{
    auto pBase = std::make_unique<BaseManager>(
        rDataContext.buildingRegistry.get(),
        rDataContext.popTypeRegistry.get(),
        rDataContext.popCompositionCalculator.get());
    pBase->SetFactionId(factionId);
    pBase->SetBaseId(baseId);
    pBase->SetName(name);
    pBase->SetPosition(x, y);

    if (tileLookup)
    {
        pBase->SetTileLookup(std::move(tileLookup));
    }

    pBase->GetWorkerAssignments().UnassignAll();
    pBase->AutoAssignWorkers();

    std::cout << "Created base '" << name << "' with population: " << pBase->GetBaseSize()
              << " (workers: " << pBase->GetPopWorkerCount() << ")\n";

    BaseManager* pRawBase = pBase.get();
    AddBase(std::move(pBase));
    return pRawBase;
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

void Faction::CollectBaseResources()
{
    for (const auto& pBase : m_bases)
    {
        if (pBase)
        {
            pBase->CollectResources(m_pEconomy.get());
        }
    }
}

void Faction::AddResearchPoints(int points)
{
    if (m_pResearch)
    {
        m_pResearch->AddResearchPoints(points);
    }
}

int Faction::GetResearchPoints() const
{
    return m_pResearch ? m_pResearch->GetAccumulatedPoints() : 0;
}

} // namespace ac
