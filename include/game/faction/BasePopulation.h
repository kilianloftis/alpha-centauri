#pragma once

#include "game/faction/Population.h"
#include "game/faction/Pop.h"
#include <memory>
#include <vector>

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

    // Pop access
    const std::vector<std::unique_ptr<Pop>>& GetPops() const;
    Pop* GetPop(size_t index);

    // Pop counts by type
    int GetWorkerCount() const;
    int GetTalentCount() const;
    int GetDroneCount() const;
    int GetSpecialistCount() const;

    // Pop manipulation
    void AddPop(std::unique_ptr<Pop> pPop);
    std::unique_ptr<Pop> RemovePop(size_t index);

    // Convert a pop from one type to another
    void ConvertToWorker(size_t index);
    void ConvertToTalent(size_t index);
    void ConvertToDrone(size_t index);
    void ConvertToSpecialist(size_t index, std::unique_ptr<Specialist> pSpecialist);

    // Drone and talent calculations
    int CalculateDroneCount(int basePopulation, int psychOutput, int factionDroneModifier) const override;
    int CalculateTalentCount(int basePopulation, int psychOutput, int factionTalentModifier) const override;
    bool HasDroneRiot() const override;
    bool IsDestroyed() const override;

    // Add a drone (for faction base count mechanic)
    void AddRandomDrone();

    // Recalculate drones and talents based on current conditions
    void RecalculateDronesAndTalents(int psychOutput, int factionDroneModifier, int factionTalentModifier);

    // Population limits
    int GetMaxSize() const;
    void SetMaxSize(int maxSize);

private:
    std::vector<std::unique_ptr<Pop>> m_pops;
    int m_maxSize;
    int m_growthRate;

    int CountPops_(bool (*predicate)(const Pop*)) const;
};

} // namespace ac
