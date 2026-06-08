#pragma once

#include "game/faction/population/Pop.h"
#include "game/faction/population/PopManager.h"
#include "lib/Signal.h"
#include <memory>
#include <vector>

namespace ac
{

class BasePopulation
{
public:
    BasePopulation();
    explicit BasePopulation(int initialSize);
    ~BasePopulation();

    // Population size management
    int GetSize() const;
    void SetSize(int size);
    int GetGrowthRate() const;
    bool CanGrow() const;

    // Pop access
    const std::vector<std::unique_ptr<Pop>>& GetPops() const;
    Pop* GetPop(size_t index);

    // Pop counts by type
    int GetWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Pop manipulation - no arguments, uses PopManager internally
    void AddPop();
    void RemovePop();

    // Convert a pop from one type to another
    void ConvertToWorker(size_t index);
    void ConvertToTalent(size_t index);
    void ConvertToDrone(size_t index);
    void ConvertToSpecialist(size_t index, std::unique_ptr<Specialist> pSpecialist);

    // Drone and talent calculations
    int CalculateDroneCount(int basePopulation, int psychOutput, int factionDroneModifier) const;
    int CalculateTalentCount(int basePopulation, int psychOutput, int factionTalentModifier) const;
    bool HasDroneRiot() const;
    bool IsDestroyed() const;

    // Add a drone (for faction base count mechanic)
    void AddRandomDrone();

    // Recalculate drones and talents based on current conditions
    void RecalculateDronesAndTalents(int psychOutput, int factionDroneModifier, int factionTalentModifier);

    // Population limits
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

    // Signals
    Signal<int> on_pop_gained;   // new size
    Signal<int> on_pop_lost;     // new size

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    std::unique_ptr<PopManager> m_pPopManager;
    int m_size;
    int m_maxSize;
    int m_growthRate;

    int CountPops_(bool (*predicate)(const Pop*)) const;

    void NotifyPopGained_();
    void NotifyPopLost_();
};

} // namespace ac
