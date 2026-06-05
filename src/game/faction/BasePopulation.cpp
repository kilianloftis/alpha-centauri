#include "game/faction/BasePopulation.h"

namespace ac
{

BasePopulation::BasePopulation()
    : Population()
    , m_maxSize(8)
    , m_growthRate(1)
{
    m_size = 1;
    m_workerCounts[WorkerRole::Worker] = 1;
}

BasePopulation::BasePopulation(int initialSize)
    : Population()
    , m_maxSize(8)
    , m_growthRate(1)
{
    m_size = initialSize;
    m_workerCounts[WorkerRole::Worker] = initialSize;
}

BasePopulation::~BasePopulation()
{
}

int BasePopulation::GetSize() const
{
    return m_size;
}

void BasePopulation::SetSize(int size)
{
    m_size = size;
    ValidateWorkerCounts_();
}

int BasePopulation::GetGrowthRate() const
{
    return m_growthRate;
}

void BasePopulation::Grow()
{
    if (CanGrow())
    {
        m_size++;
        m_workerCounts[WorkerRole::Worker]++;
    }
}

bool BasePopulation::CanGrow() const
{
    return m_size < m_maxSize;
}

int BasePopulation::GetWorkerCount(WorkerRole role) const
{
    auto it = m_workerCounts.find(role);
    if (it != m_workerCounts.end())
    {
        return it->second;
    }
    return 0;
}

void BasePopulation::SetWorkerCount(WorkerRole role, int count)
{
    m_workerCounts[role] = count;
    ValidateWorkerCounts_();
}

int BasePopulation::GetTotalWorkers() const
{
    int total = 0;
    for (const auto& pair : m_workerCounts)
    {
        total += pair.second;
    }
    return total;
}

bool BasePopulation::AssignWorker(WorkerRole role)
{
    int currentWorkers = GetTotalWorkers();
    if (currentWorkers >= m_size)
    {
        return false;
    }

    m_workerCounts[role]++;
    return true;
}

bool BasePopulation::UnassignWorker(WorkerRole role)
{
    if (m_workerCounts[role] > 0)
    {
        m_workerCounts[role]--;
        return true;
    }
    return false;
}

int BasePopulation::GetMaxSize() const
{
    return m_maxSize;
}

void BasePopulation::SetMaxSize(int maxSize)
{
    m_maxSize = maxSize;
    ValidateWorkerCounts_();
}

void BasePopulation::ValidateWorkerCounts_()
{
    int totalWorkers = GetTotalWorkers();
    
    // If total workers exceed population size, reduce workers
    while (totalWorkers > m_size && totalWorkers > 0)
    {
        // Reduce from roles that have workers
        for (auto& pair : m_workerCounts)
        {
            if (pair.second > 0)
            {
                pair.second--;
                totalWorkers--;
                break;
            }
        }
    }
}

} // namespace ac
