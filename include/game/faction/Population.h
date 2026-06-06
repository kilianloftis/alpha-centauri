#pragma once

namespace ac
{

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

    // Drone and talent calculations (override in derived classes)
    virtual int CalculateDroneCount(int basePopulation, int psychOutput, int factionDroneModifier) const = 0;
    virtual int CalculateTalentCount(int basePopulation, int psychOutput, int factionTalentModifier) const = 0;
    virtual bool HasDroneRiot() const = 0;
    virtual bool IsDestroyed() const = 0;

protected:
    int m_size;
};

} // namespace ac
