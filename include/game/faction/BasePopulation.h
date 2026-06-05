#pragma once

#include "game/faction/Population.h"
#include <map>

namespace ac
{

class BasePopulation : public Population
{
public:
    BasePopulation();
    explicit BasePopulation(int initialSize);
    ~BasePopulation() override;

    // Population size management
    int GetSize() const override;
    void SetSize(int size) override;
    int GetGrowthRate() const override;
    void Grow() override;
    bool CanGrow() const override;

    // Worker role management
    int GetWorkerCount(WorkerRole role) const override;
    void SetWorkerCount(WorkerRole role, int count) override;
    int GetTotalWorkers() const override;

    // Role assignment
    bool AssignWorker(WorkerRole role) override;
    bool UnassignWorker(WorkerRole role) override;

    // Population limits
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

private:
    std::map<WorkerRole, int> m_workerCounts;
    int m_maxSize;
    int m_growthRate;

    void ValidateWorkerCounts_();
};

} // namespace ac
