#include "game/faction/base/population/PopContainer.h"
#include "game/faction/ResearchManager.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include <stdexcept>

namespace ac
{

PopContainer::PopContainer(const PopTypeRegistry& rRegistry,
                           const PopTypeAvailabilityCalculator& rAvailabilityCalculator,
                           const ResearchManager& rResearchManager,
                           int initialSize)
    : m_rRegistry(rRegistry)
    , m_rAvailabilityCalculator(rAvailabilityCalculator)
    , m_pResearchManager(&rResearchManager)
{
    const std::string& rDefaultId = m_rRegistry.GetDefault().id;
    for (int i = 0; i < initialSize; ++i)
    {
        m_pops.push_back(m_rRegistry.Create(rDefaultId));
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
    return CountPops_([](const Pop* p) { return p->IsTalent(); });
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
    auto pPop = m_rRegistry.Create(typeId);
    m_pops.push_back(std::move(pPop));
    m_revision.Bump();
}

void PopContainer::RemovePop()
{
    if (!m_pops.empty())
    {
        m_pops.pop_back();
        m_revision.Bump();
    }
}

void PopContainer::ConvertTo(Pop& rPop, const std::string& typeId)
{
    const PopTypeConfig_t* pConfig = m_rRegistry.Find(typeId);
    if (!pConfig)
    {
        throw std::runtime_error("Unknown pop type: " + typeId);
    }
    rPop.Convert(*pConfig);
    m_revision.Bump();
}

void PopContainer::ConvertToFallback(Pop& rPop)
{
    const PopTypeConfig_t* pCurrentConfig = m_rRegistry.Find(rPop.GetPopType());
    if (!pCurrentConfig)
    {
        throw std::runtime_error("Current pop type not found in registry: " + std::string(rPop.GetPopType()));
    }
    if (pCurrentConfig->fallbackPopTypeId.empty())
    {
        throw std::runtime_error("Pop has no fallback type configured");
    }
    const PopTypeConfig_t& rResolved = m_rAvailabilityCalculator.ResolveCurrentType(
        pCurrentConfig->fallbackPopTypeId, m_pResearchManager->GetDiscoveredTechs());
    rPop.Convert(rResolved);
    m_revision.Bump();
}

void PopContainer::ApplyCompositionTargets(const PopCompositionResult& targets,
                                           const std::string& defaultTypeId,
                                           const std::string& droneTypeId,
                                           const std::string& talentTypeId)
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
        if (pPop->IsTalent())
        {
            ConvertTo(*pPop, defaultTypeId);
            currentTalents--;
        }
    }

    // Convert plain workers to drones to reach target
    currentDrones = GetDroneCount();
    for (auto& pPop : m_pops)
    {
        if (currentDrones >= targets.targetDrones) break;
        if (pPop->IsPlainWorker())
        {
            ConvertTo(*pPop, droneTypeId);
            currentDrones++;
        }
    }

    // Convert plain workers to talents to reach target
    currentTalents = GetTalentCount();
    for (auto& pPop : m_pops)
    {
        if (currentTalents >= targets.targetTalents) break;
        if (pPop->IsPlainWorker())
        {
            ConvertTo(*pPop, talentTypeId);
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

void PopContainer::RebindResearch(const ResearchManager& rResearch)
{
    m_pResearchManager = &rResearch;
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
