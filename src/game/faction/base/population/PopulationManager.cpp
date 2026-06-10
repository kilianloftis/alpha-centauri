#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/population/calculators/PopCompositionCalculator.h"
#include "game/faction/base/population/pop-types/PopTypeConfigParser.h"
#include "game/faction/base/population/pop-types/PopTypeRegistry.h"

namespace ac
{

PopulationManager::PopulationManager()
    : PopulationManager(3)
{
}

PopulationManager::PopulationManager(int initialSize)
    : m_maxSize(8)
    , m_growthRate(1)
    , m_riot(on_will_riot, on_is_rioting, on_riot_ended)
    , m_growth(on_growth, on_starvation)
    , m_golden_age(on_golden_age_started, on_golden_age_ended)
{
    on_growth.connect([this]() { AddPop(); });
    on_starvation.connect([this]() { RemovePop(); });

    if (initialSize > 0)
    {
        // Reserve capacity in container for initial population
        // Actual pops created when registry is set
        m_container.Reserve(initialSize);
    }
}

PopulationManager::~PopulationManager()
{
}

int PopulationManager::GetSize() const
{
    return m_container.GetSize();
}

const std::string& PopulationManager::GetDefaultPopType_() const
{
    static const std::string kFallback = "Worker";
    if (m_pCompositionCalculator)
    {
        return m_pCompositionCalculator->GetConfig().defaultType;
    }
    return kFallback;
}

int PopulationManager::GetGrowthRate() const
{
    return m_growthRate;
}

bool PopulationManager::CanGrow() const
{
    return m_container.GetSize() < m_maxSize;
}

void PopulationManager::AddPop()
{
    if (CanGrow())
    {
        m_container.AddPop(GetDefaultPopType_());
        NotifyPopGained_();
        m_riot.NotifyPopGrown(BuildRiotInputs_());
    }
}

void PopulationManager::RemovePop()
{
    m_container.RemovePop();
    NotifyPopLost_();
}

void PopulationManager::ConvertTo(size_t index, const std::string& typeId)
{
    m_container.ConvertTo(index, typeId);
}

int PopulationManager::GetMaxSize() const
{
    return m_maxSize;
}

void PopulationManager::SetMaxSize(int maxSize)
{
    m_maxSize = maxSize;
    // Trim excess pops if max size decreased
    while (m_container.GetSize() > m_maxSize)
    {
        RemovePop();
    }
}

bool PopulationManager::IsRioting() const
{
    return m_riot.IsRioting();
}

bool PopulationManager::IsDestroyed() const
{
    return m_container.GetSize() == 0;
}

RiotConditionInputs PopulationManager::BuildRiotInputs_() const
{
    RiotConditionInputs inputs;
    inputs.droneCount = m_container.GetDroneCount();
    inputs.talentCount = m_container.GetTalentCount();
    if (m_pCompositionCalculator)
    {
        PopCompositionInputs compInputs;
        compInputs.baseSize = m_container.GetSize();
        compInputs.psychOutput = m_container.ComputePsychOutput();
        const PopCompositionResult result = m_pCompositionCalculator->Calculate(compInputs);
        inputs.targetTalents = result.targetTalents;
    }
    return inputs;
}

void PopulationManager::SetRegistry(const PopTypeRegistry* pRegistry)
{
    const int popsCreated = m_container.SetRegistry(pRegistry);
    for (int i = 0; i < popsCreated; ++i)
    {
        NotifyPopGained_();
    }
}

void PopulationManager::SetCompositionCalculator(PopCompositionCalculator* pCalculator)
{
    m_pCompositionCalculator = pCalculator;
}

void PopulationManager::RecalculateComposition()
{
    if (!m_pCompositionCalculator)
    {
        return;
    }

    PopCompositionInputs inputs;
    inputs.baseSize = m_container.GetSize();
    inputs.psychOutput = m_container.ComputePsychOutput();
    // TODO: supply faction drone/talent modifiers once faction modifiers are accessible here
    const PopCompositionResult targets = m_pCompositionCalculator->Calculate(inputs);

    m_container.ApplyCompositionTargets(targets, GetDefaultPopType_());
}

void PopulationManager::AccumulateGrowth(int nutrientsPerTurn)
{
    GrowthInputs_t inputs;
    inputs.baseSize = m_container.GetSize();
    inputs.nutrientsPerTurn = nutrientsPerTurn;
    inputs.growthRateModifier = m_growthRate;
    m_growth.Accumulate(inputs);
}

void PopulationManager::CheckRiotEndOfTurn()
{
    m_riot.Update(BuildRiotInputs_());
}

void PopulationManager::CheckGoldenAgeEndOfTurn()
{
    GoldenAgeCalculator::Inputs_t inputs;
    inputs.droneCount = m_container.GetDroneCount();
    inputs.talentCount = m_container.GetTalentCount();
    inputs.workerCount = m_container.GetWorkerCount();
    inputs.specialistCount = m_container.GetSpecialistCount();
    m_golden_age.Update(inputs);
}

int PopulationManager::GetNutrientBank() const
{
    return m_growth.GetNutrientBank();
}

void PopulationManager::NotifyPopGained_()
{
    on_pop_gained.emit(GetSize());
}

void PopulationManager::NotifyPopLost_()
{
    on_pop_lost.emit(GetSize());
}

} // namespace ac
