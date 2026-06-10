#include "game/faction/base/population/PopContainer.h"
#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include "game/faction/base/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/population/pop-types/PopTypeRegistry.h"

namespace ac
{

PopContainer::PopContainer()
    : m_pPopFactory(std::make_unique<PopFactory>())
    , m_nextPopId(0)
{
}

int PopContainer::GetSize() const
{
    return static_cast<int>(m_pops.size());
}

const std::vector<std::unique_ptr<Pop>>& PopContainer::GetPops() const
{
    return m_pops;
}

Pop* PopContainer::GetPop(size_t index)
{
    if (index < m_pops.size())
    {
        return m_pops[index].get();
    }
    return nullptr;
}

int PopContainer::GetWorkerCount() const
{
    return CountPops_([](const Pop* p) { return p->IsWorker() && !p->IsSpecialist(); });
}

int PopContainer::GetTalentCount() const
{
    return CountPops_([](const Pop* p) { return p->IsWorker() && p->GetGoldenAgeContribution() > 0; });
}

int PopContainer::GetDroneCount() const
{
    return CountPops_([](const Pop* p) { return p->IsDrone(); });
}

int PopContainer::GetSpecialistCount() const
{
    return CountPops_([](const Pop* p) { return p->IsSpecialist(); });
}

void PopContainer::Reserve(int count)
{
    m_pops.reserve(count);
}

void PopContainer::AddPop(const std::string& typeId)
{
    auto pPop = m_pPopFactory->CreatePop(typeId);
    if (pPop)
    {
        pPop->SetId(m_nextPopId++);
        m_pops.push_back(std::move(pPop));
    }
}

void PopContainer::RemovePop()
{
    if (!m_pops.empty())
    {
        m_pops.pop_back();
    }
}

void PopContainer::ConvertTo(size_t index, const std::string& typeId)
{
    if (index >= m_pops.size())
    {
        return;
    }
    int popId  = m_pops[index]->GetId();
    int tileId = m_pops[index]->GetTileId();
    auto pNewPop = m_pPopFactory->CreatePop(typeId);
    if (pNewPop)
    {
        pNewPop->SetId(popId);
        pNewPop->SetTileId(tileId);
        m_pops[index] = std::move(pNewPop);
    }
}

void PopContainer::PromoteWorkerToDrone()
{
    for (size_t i = 0; i < m_pops.size(); i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist())
        {
            ConvertTo(i, "Drone");
            return;
        }
    }
}

void PopContainer::ApplyCompositionTargets(const PopCompositionResult& targets, const std::string& defaultTypeId)
{
    // Convert excess drones back to workers first
    int currentDrones = GetDroneCount();
    for (size_t i = 0; i < m_pops.size() && currentDrones > targets.targetDrones; i++)
    {
        if (m_pops[i]->IsDrone())
        {
            ConvertTo(i, defaultTypeId);
            currentDrones--;
        }
    }

    // Convert excess talents back to workers
    int currentTalents = GetTalentCount();
    for (size_t i = 0; i < m_pops.size() && currentTalents > targets.targetTalents; i++)
    {
        if (m_pops[i]->IsWorker() && m_pops[i]->GetGoldenAgeContribution() > 0)
        {
            ConvertTo(i, defaultTypeId);
            currentTalents--;
        }
    }

    // Convert workers to drones to reach target
    currentDrones = GetDroneCount();
    for (size_t i = 0; i < m_pops.size() && currentDrones < targets.targetDrones; i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist() && m_pops[i]->GetGoldenAgeContribution() == 0)
        {
            ConvertTo(i, "Drone");
            currentDrones++;
        }
    }

    // Convert workers to talents to reach target
    currentTalents = GetTalentCount();
    for (size_t i = 0; i < m_pops.size() && currentTalents < targets.targetTalents; i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist() && m_pops[i]->GetGoldenAgeContribution() == 0)
        {
            ConvertTo(i, "Talent");
            currentTalents++;
        }
    }
}

int PopContainer::SetRegistry(const PopTypeRegistry* pRegistry)
{
    m_pPopFactory->SetRegistry(pRegistry);

    int popsCreated = 0;

    // Populate reserved pops now that the registry is available
    if (m_pops.empty() && m_pops.capacity() > 0)
    {
        const size_t target = m_pops.capacity();
        for (size_t i = 0; i < target; i++)
        {
            // Default type will be resolved when needed
            auto pPop = m_pPopFactory->CreatePop("Worker");
            if (pPop)
            {
                pPop->SetId(m_nextPopId++);
                m_pops.push_back(std::move(pPop));
                ++popsCreated;
            }
        }
    }

    return popsCreated;
}

int PopContainer::ComputePsychOutput() const
{
    int total = 0;
    for (const auto& pPop : m_pops)
    {
        total += pPop->GetSpecialistOutput().psych;
    }
    return total;
}

int PopContainer::CountPops_(bool (*predicate)(const Pop*)) const
{
    int count = 0;
    for (const auto& pPop : m_pops)
    {
        if (predicate(pPop.get()))
        {
            count++;
        }
    }
    return count;
}

} // namespace ac
