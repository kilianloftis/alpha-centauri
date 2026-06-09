#pragma once

#include "game/faction/population/Pop.h"
#include "game/faction/population/PopManager.h"
#include "game/faction/population/PopCompositionCalculator.h"
#include "lib/Signal.h"
#include <memory>
#include <string>
#include <vector>

namespace ac
{

class PopTypeRegistry;

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

    // Convert a pop to any type by config id (e.g. "Worker", "Drone", "Talent", "Librarian")
    void ConvertTo(size_t index, const std::string& typeId);

    // Drone and talent calculations
    bool HasDroneRiot() const;
    bool IsDestroyed() const;

    // Add a drone (for faction base count mechanic)
    void AddDrone();

    // Registry injection — forwarded to the internal PopManager
    void SetRegistry(const PopTypeRegistry* pRegistry);

    // Composition calculator injection
    void SetCompositionCalculator(PopCompositionCalculator* pCalculator);

    // Population limits
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

    // Signals
    Signal<int> on_pop_gained;   // new size
    Signal<int> on_pop_lost;     // new size

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    std::unique_ptr<PopManager> m_pPopManager;
    PopCompositionCalculator* m_pCompositionCalculator = nullptr;
    int m_size;
    int m_maxSize;
    int m_growthRate;

    int CountPops_(bool (*predicate)(const Pop*)) const;

    void NotifyPopGained_();
    void NotifyPopLost_();
};

} // namespace ac
