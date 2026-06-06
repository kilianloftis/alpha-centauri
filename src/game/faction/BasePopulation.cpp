#include "game/faction/BasePopulation.h"
#include "game/faction/Specialist.h"

namespace ac
{

BasePopulation::BasePopulation()
    : Population()
    , m_maxSize(8)
    , m_growthRate(1)
{
    m_size = 1;
    // Start with one worker
    m_pops.push_back(std::make_unique<WorkerPop>());
}

BasePopulation::BasePopulation(int initialSize)
    : Population()
    , m_maxSize(8)
    , m_growthRate(1)
{
    m_size = initialSize;
    if (m_size < 0)
    {
        m_size = 0;
    }
    // Initialize with workers
    for (int i = 0; i < m_size; i++)
    {
        m_pops.push_back(std::make_unique<WorkerPop>());
    }
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
    if (size < 0)
    {
        size = 0;
    }

    // Remove pops if size decreased
    while (static_cast<int>(m_pops.size()) > size)
    {
        m_pops.pop_back();
    }

    // Add workers if size increased
    while (static_cast<int>(m_pops.size()) < size)
    {
        m_pops.push_back(std::make_unique<WorkerPop>());
    }

    m_size = static_cast<int>(m_pops.size());
}

int BasePopulation::GetGrowthRate() const
{
    return m_growthRate;
}

void BasePopulation::Grow()
{
    if (CanGrow())
    {
        m_pops.push_back(std::make_unique<WorkerPop>());
        m_size = static_cast<int>(m_pops.size());
    }
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
    // Talents are workers that are also marked as talents
    int count = 0;
    for (const auto& pPop : m_pops)
    {
        // Dynamic cast to check if it's specifically a TalentPop
        if (dynamic_cast<TalentPop*>(pPop.get()) != nullptr)
        {
            count++;
        }
    }
    return count;
}

int BasePopulation::GetDroneCount() const
{
    return CountPops_([](const Pop* p) { return p->IsDrone(); });
}

int BasePopulation::GetSpecialistCount() const
{
    return CountPops_([](const Pop* p) { return p->IsSpecialist(); });
}

void BasePopulation::AddPop(std::unique_ptr<Pop> pPop)
{
    if (pPop && static_cast<int>(m_pops.size()) < m_maxSize)
    {
        m_pops.push_back(std::move(pPop));
        m_size = static_cast<int>(m_pops.size());
    }
}

std::unique_ptr<Pop> BasePopulation::RemovePop(size_t index)
{
    if (index < m_pops.size())
    {
        std::unique_ptr<Pop> removed = std::move(m_pops[index]);
        m_pops.erase(m_pops.begin() + index);
        m_size = static_cast<int>(m_pops.size());
        return removed;
    }
    return nullptr;
}

void BasePopulation::ConvertToWorker(size_t index)
{
    if (index < m_pops.size())
    {
        int tileId = m_pops[index]->IsWorker() ? dynamic_cast<WorkerPop*>(m_pops[index].get())->GetTileId() : -1;
        m_pops[index] = std::make_unique<WorkerPop>();
        if (WorkerPop* pWorker = dynamic_cast<WorkerPop*>(m_pops[index].get()))
        {
            pWorker->SetTileId(tileId);
        }
    }
}

void BasePopulation::ConvertToTalent(size_t index)
{
    if (index < m_pops.size())
    {
        int tileId = m_pops[index]->IsWorker() ? dynamic_cast<WorkerPop*>(m_pops[index].get())->GetTileId() : -1;
        m_pops[index] = std::make_unique<TalentPop>();
        if (WorkerPop* pWorker = dynamic_cast<WorkerPop*>(m_pops[index].get()))
        {
            pWorker->SetTileId(tileId);
        }
    }
}

void BasePopulation::ConvertToDrone(size_t index)
{
    if (index < m_pops.size())
    {
        m_pops[index] = std::make_unique<DronePop>();
    }
}

void BasePopulation::ConvertToSpecialist(size_t index, std::unique_ptr<Specialist> pSpecialist)
{
    if (index < m_pops.size() && pSpecialist)
    {
        m_pops[index] = std::make_unique<SpecialistPop>(std::move(pSpecialist));
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
    while (static_cast<int>(m_pops.size()) > m_maxSize)
    {
        m_pops.pop_back();
    }
    m_size = static_cast<int>(m_pops.size());
}

int BasePopulation::CalculateDroneCount(int basePopulation, int psychOutput, int factionDroneModifier) const
{
    // Formula: Drones = basePopulation / 4 - psychOutput / 4 + factionDroneModifier
    int droneCount = (basePopulation / 4) - (psychOutput / 4) + factionDroneModifier;
    return droneCount > 0 ? droneCount : 0;
}

int BasePopulation::CalculateTalentCount(int basePopulation, int psychOutput, int factionTalentModifier) const
{
    // Formula: Talents = psychOutput / 4 + factionTalentModifier
    int talentCount = (psychOutput / 4) + factionTalentModifier;
    return talentCount > 0 ? talentCount : 0;
}

bool BasePopulation::HasDroneRiot() const
{
    return GetDroneCount() > GetTalentCount();
}

bool BasePopulation::IsDestroyed() const
{
    return m_pops.empty();
}

void BasePopulation::AddRandomDrone()
{
    // Convert a random worker to a drone
    for (size_t i = 0; i < m_pops.size(); i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist())
        {
            ConvertToDrone(i);
            return;
        }
    }
}

void BasePopulation::RecalculateDronesAndTalents(int psychOutput, int factionDroneModifier, int factionTalentModifier)
{
    int currentSize = static_cast<int>(m_pops.size());
    int targetDrones = CalculateDroneCount(currentSize, psychOutput, factionDroneModifier);
    int targetTalents = CalculateTalentCount(currentSize, psychOutput, factionTalentModifier);

    int currentDrones = GetDroneCount();
    int currentTalents = GetTalentCount();

    // First pass: convert excess drones back to workers
    if (currentDrones > targetDrones)
    {
        int toConvert = currentDrones - targetDrones;
        for (size_t i = 0; i < m_pops.size() && toConvert > 0; i++)
        {
            if (m_pops[i]->IsDrone())
            {
                ConvertToWorker(i);
                toConvert--;
            }
        }
    }

    // Second pass: convert excess talents back to workers
    if (currentTalents > targetTalents)
    {
        int toConvert = currentTalents - targetTalents;
        for (size_t i = 0; i < m_pops.size() && toConvert > 0; i++)
        {
            if (dynamic_cast<TalentPop*>(m_pops[i].get()) != nullptr)
            {
                ConvertToWorker(i);
                toConvert--;
            }
        }
    }

    // Third pass: convert workers to drones as needed
    if (currentDrones < targetDrones)
    {
        int toConvert = targetDrones - currentDrones;
        for (size_t i = 0; i < m_pops.size() && toConvert > 0; i++)
        {
            // Only convert regular workers, not talents or specialists
            if (m_pops[i]->IsWorker() && !dynamic_cast<TalentPop*>(m_pops[i].get()) && !m_pops[i]->IsSpecialist())
            {
                ConvertToDrone(i);
                toConvert--;
            }
        }
    }

    // Fourth pass: convert workers to talents as needed
    if (currentTalents < targetTalents)
    {
        int toConvert = targetTalents - currentTalents;
        for (size_t i = 0; i < m_pops.size() && toConvert > 0; i++)
        {
            // Only convert regular workers, not specialists
            if (m_pops[i]->IsWorker() && !dynamic_cast<TalentPop*>(m_pops[i].get()) && !m_pops[i]->IsSpecialist())
            {
                ConvertToTalent(i);
                toConvert--;
            }
        }
    }
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

} // namespace ac
