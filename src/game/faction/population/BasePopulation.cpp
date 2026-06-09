#include "game/faction/population/BasePopulation.h"
#include "game/faction/population/PopTypeRegistry.h"

namespace ac
{

BasePopulation::BasePopulation()
    : m_pPopManager(std::make_unique<PopManager>())
    , m_size(1)
    , m_maxSize(8)
    , m_growthRate(1)
{
}

BasePopulation::BasePopulation(int initialSize)
    : m_pPopManager(std::make_unique<PopManager>())
    , m_size(initialSize > 0 ? initialSize : 0)
    , m_maxSize(8)
    , m_growthRate(1)
{
}

BasePopulation::~BasePopulation()
{
}

int BasePopulation::GetSize() const
{
    return static_cast<int>(m_pops.size());
}

void BasePopulation::SetSize(int size)
{
    if (size > GetSize())
    {
        // Add pops
        while (GetSize() < size)
        {
            AddPop();
        }
    }
    else if (size < GetSize())
    {
        // Remove pops
        while (GetSize() > size)
        {
            RemovePop();
        }
    }
}

int BasePopulation::GetGrowthRate() const
{
    return m_growthRate;
}

bool BasePopulation::CanGrow() const
{
    return static_cast<int>(m_pops.size()) < m_maxSize;
}

const std::vector<std::unique_ptr<Pop>>& BasePopulation::GetPops() const
{
    return m_pops;
}

Pop* BasePopulation::GetPop(size_t index)
{
    if (index < m_pops.size())
    {
        return m_pops[index].get();
    }
    return nullptr;
}

int BasePopulation::GetWorkerCount() const
{
    return CountPops_([](const Pop* p) { return p->IsWorker() && !p->IsSpecialist(); });
}

int BasePopulation::GetTalentCount() const
{
    return CountPops_([](const Pop* p) { return p->IsWorker() && p->GetGoldenAgeContribution() > 0; });
}

int BasePopulation::GetDroneCount() const
{
    return CountPops_([](const Pop* p) { return p->IsDrone(); });
}

int BasePopulation::GetSpecialistCount() const
{
    return CountPops_([](const Pop* p) { return p->IsSpecialist(); });
}

void BasePopulation::AddPop()
{
    if (CanGrow())
    {
        // Use PopManager to create the pop (currently always a worker)
        m_pops.push_back(m_pPopManager->CreatePop());
        m_size = static_cast<int>(m_pops.size());
        NotifyPopGained_();
    }
}

void BasePopulation::RemovePop()
{
    if (!m_pops.empty())
    {
        m_pops.pop_back();
        m_size = static_cast<int>(m_pops.size());
        NotifyPopLost_();
    }
}

void BasePopulation::ConvertTo(size_t index, const std::string& typeId)
{
    if (index >= m_pops.size())
    {
        return;
    }
    int tileId = m_pops[index]->GetTileId();
    auto pNewPop = m_pPopManager->CreatePop(typeId);
    if (pNewPop)
    {
        pNewPop->SetTileId(tileId);
        m_pops[index] = std::move(pNewPop);
    }
}

void BasePopulation::SetRegistry(const PopTypeRegistry* pRegistry)
{
    m_pPopManager->SetRegistry(pRegistry);

    // Populate initial pops now that the registry is available
    if (m_pops.empty())
    {
        for (int i = 0; i < m_size; i++)
        {
            auto pPop = m_pPopManager->CreatePop();
            if (pPop)
            {
                m_pops.push_back(std::move(pPop));
            }
        }
        m_size = static_cast<int>(m_pops.size());
    }
}

int BasePopulation::GetMaxSize() const
{
    return m_maxSize;
}

void BasePopulation::SetMaxSize(int maxSize)
{
    m_maxSize = maxSize;
    // Trim excess pops if max size decreased
    bool bLostPop = false;
    while (static_cast<int>(m_pops.size()) > m_maxSize)
    {
        m_pops.pop_back();
        bLostPop = true;
    }
    m_size = static_cast<int>(m_pops.size());
    if (bLostPop)
    {
        NotifyPopLost_();
    }
}

bool BasePopulation::HasDroneRiot() const
{
    return GetDroneCount() > GetTalentCount();
}

bool BasePopulation::IsDestroyed() const
{
    return m_pops.empty();
}

void BasePopulation::AddDrone()
{
    // Convert a random worker to a drone
    for (size_t i = 0; i < m_pops.size(); i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist())
        {
            ConvertTo(i, "Drone");
            return;
        }
    }
}

void BasePopulation::SetCompositionCalculator(PopCompositionCalculator* pCalculator)
{
    m_pCompositionCalculator = pCalculator;
}

int BasePopulation::CountPops_(bool (*predicate)(const Pop*)) const
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

void BasePopulation::NotifyPopGained_()
{
    on_pop_gained.emit(m_size);
}

void BasePopulation::NotifyPopLost_()
{
    on_pop_lost.emit(m_size);
}

} // namespace ac
