#pragma once

#include <map>

namespace ac
{

enum class WorkerRole
{
    Worker,     // Works tiles (produces nutrients, energy, minerals)
    Lab,        // Contributes to research
    Psych,      // Contributes to psych
    Econ,       // Contributes to energy directly
    Drone,      // Doesn't work tiles or produce anything
    Talent      // Normal worker (same as Worker for now)
};

class Population
{
public:
    Population();
    virtual ~Population();

    // Pure virtual methods for population behavior
    virtual int GetSize() const = 0;
    virtual void SetSize(int size) = 0;
    virtual int GetGrowthRate() const = 0;
    virtual void Grow() = 0;
    virtual bool CanGrow() const = 0;

    // Worker role management
    virtual int GetWorkerCount(WorkerRole role) const = 0;
    virtual void SetWorkerCount(WorkerRole role, int count) = 0;
    virtual int GetTotalWorkers() const = 0;

    // Role assignment
    virtual bool AssignWorker(WorkerRole role) = 0;
    virtual bool UnassignWorker(WorkerRole role) = 0;

protected:
    int m_size;
};

} // namespace ac
