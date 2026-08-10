#include "game/faction/base/population/PopContainer.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include <algorithm>
#include <stdexcept>

namespace ac
{

PopContainer::PopContainer(const PopTypeRegistry& rRegistry, int initialSize)
    : m_rRegistry(rRegistry)
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
    // Every tile-capable pop: plain workers *plus* drones and talents. PopTypeRegistry enforces
    // role Specialist <=> !can_work_tile, so IsWorker() already excludes specialists.
    return CountPops_([](const Pop* p) { return p->IsWorker(); });
}

int PopContainer::GetPlainWorkerCount() const
{
    return CountPops_([](const Pop* p) { return p->IsPlainWorker(); });
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

int PopContainer::GetRiotContribution() const
{
    int total = 0;
    for (const auto& pPop : m_pops)
    {
        total += pPop->GetRiotContribution();
    }
    return total;
}

void PopContainer::AddPop(const PopTypeConfig_t& rConfig)
{
    m_pops.push_back(m_rRegistry.Create(rConfig.id));
    m_revision.Bump();
}

void PopContainer::Remove(Pop& rPop)
{
    const auto it = std::find_if(m_pops.begin(), m_pops.end(),
        [&rPop](const std::unique_ptr<Pop>& pPop) { return pPop.get() == &rPop; });
    if (it == m_pops.end())
    {
        throw std::runtime_error("PopContainer::Remove: pop does not belong to this base");
    }
    m_pops.erase(it);
    m_revision.Bump();
}

void PopContainer::ConvertTo(Pop& rPop, const PopTypeConfig_t& rConfig)
{
    rPop.Convert(rConfig);
    m_revision.Bump();
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
