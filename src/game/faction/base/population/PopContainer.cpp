#include "game/faction/base/population/PopContainer.h"
#include "game/faction/ResearchManager.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include <stdexcept>

namespace ac
{

PopContainer::PopContainer(const PopTypeRegistry* pReg,
                           const PopTypeAvailabilityCalculator* pAvailabilityCalculator,
                           const ResearchManager* pResearchManager,
                           int initialSize)
    : m_pRegistry(pReg)
    , m_pAvailabilityCalculator(pAvailabilityCalculator)
    , m_pResearchManager(pResearchManager)
{
    if (m_pRegistry && initialSize > 0)
    {
        const std::string& rDefaultId = m_pRegistry->GetDefault().id;
        for (int i = 0; i < initialSize; ++i)
        {
            m_pops.push_back(m_pRegistry->Create(rDefaultId));
        }
    }
}

int PopContainer::GetSize() const
{
    return static_cast<int>(m_pops.size());
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

void PopContainer::AddPop(const std::string& typeId)
{
    if (!m_pRegistry)
    {
        throw std::runtime_error("PopContainer has no registry");
    }
    auto pPop = m_pRegistry->Create(typeId);
    m_pops.push_back(std::move(pPop));
}

void PopContainer::RemovePop()
{
    if (!m_pops.empty())
    {
        m_pops.pop_back();
    }
}

void PopContainer::ConvertTo(Pop& rPop, const std::string& typeId)
{
    if (!m_pRegistry)
    {
        throw std::runtime_error("PopContainer has no registry");
    }
    const PopTypeConfig_t* pConfig = m_pRegistry->Find(typeId);
    if (!pConfig)
    {
        throw std::runtime_error("Unknown pop type: " + typeId);
    }
    rPop.Convert(*pConfig);
}

void PopContainer::ConvertToFallback(Pop& rPop)
{
    if (!m_pRegistry || !m_pAvailabilityCalculator || !m_pResearchManager)
    {
        throw std::runtime_error("PopContainer missing registry, availability calculator, or research manager");
    }
    const PopTypeConfig_t* pCurrentConfig = m_pRegistry->Find(rPop.GetPopType());
    if (!pCurrentConfig)
    {
        throw std::runtime_error("Current pop type not found in registry: " + std::string(rPop.GetPopType()));
    }
    if (pCurrentConfig->fallbackPopTypeId.empty())
    {
        throw std::runtime_error("Pop has no fallback type configured");
    }
    const PopTypeConfig_t& rResolved = m_pAvailabilityCalculator->ResolveCurrentType(
        pCurrentConfig->fallbackPopTypeId, m_pResearchManager->GetDiscoveredTechs());
    rPop.Convert(rResolved);
}

void PopContainer::ApplyCompositionTargets(const PopCompositionResult& targets, const std::string& defaultTypeId)
{
    // Convert excess drones back to workers first
    int currentDrones = GetDroneCount();
    for (auto& pPop : m_pops)
    {
        if (currentDrones <= targets.targetDrones)
        {
            break;
        }
        if (pPop->IsDrone())
        {
            ConvertTo(*pPop, defaultTypeId);
            currentDrones--;
        }
    }

    // Convert excess talents back to workers
    int currentTalents = GetTalentCount();
    for (auto& pPop : m_pops)
    {
        if (currentTalents <= targets.targetTalents) break;
        if (pPop->IsWorker() && pPop->GetGoldenAgeContribution() > 0)
        {
            ConvertTo(*pPop, defaultTypeId);
            currentTalents--;
        }
    }

    // Convert workers to drones to reach target
    currentDrones = GetDroneCount();
    for (auto& pPop : m_pops)
    {
        if (currentDrones >= targets.targetDrones) break;
        if (pPop->IsWorker() && !pPop->IsSpecialist() && pPop->GetGoldenAgeContribution() == 0)
        {
            ConvertTo(*pPop, "Drone");
            currentDrones++;
        }
    }

    // Convert workers to talents to reach target
    currentTalents = GetTalentCount();
    for (auto& pPop : m_pops)
    {
        if (currentTalents >= targets.targetTalents) break;
        if (pPop->IsWorker() && !pPop->IsSpecialist() && pPop->GetGoldenAgeContribution() == 0)
        {
            ConvertTo(*pPop, "Talent");
            currentTalents++;
        }
    }
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
