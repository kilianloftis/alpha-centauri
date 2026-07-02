#include "game/faction/base/population/PopulationManager.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/faction/ResearchManager.h"
#include <stdexcept>

namespace ac
{

PopulationManager::PopulationManager(const PopTypeRegistry* pReg,
                                     const PopTypeAvailabilityCalculator* pAvailabilityCalculator,
                                     const ResearchManager* pResearchManager,
                                     PopCompositionCalculator* pCalc,
                                     const GrowthConfig_t& rGrowthConfig,
                                     int initialSize)
    : m_container(pReg, pAvailabilityCalculator, pResearchManager, initialSize)
    , m_pRegistry(pReg)
    , m_rGrowthConfig(rGrowthConfig)
    , m_pCompositionCalculator(pCalc)
    , m_maxSize(8)
    , m_nutrientStockpile(0)
    , m_riot(on_will_riot, on_is_rioting, on_riot_ended)
    , m_golden_age(on_golden_age_started, on_golden_age_ended)
{
}

PopulationManager::~PopulationManager()
{
}

int PopulationManager::GetSize() const
{
    return m_container.GetSize();
}

const std::string& PopulationManager::GetDefaultPopType() const
{
    if (!m_pRegistry)
    {
        throw std::runtime_error("No PopTypeRegistry");
    }
    return m_pRegistry->GetDefault().id;
}

bool PopulationManager::CanGrow() const
{
    return m_container.GetSize() < m_maxSize;
}

void PopulationManager::AddPop()
{
    if (CanGrow())
    {
        m_container.AddPop(GetDefaultPopType());
        NotifyPopGained_();
        m_riot.NotifyPopGrown(BuildRiotInputs_());
    }
}

void PopulationManager::RemovePop()
{
    m_container.RemovePop();
    NotifyPopLost_();
}

void PopulationManager::ConvertTo(Pop& rPop, const std::string& typeId)
{
    m_container.ConvertTo(rPop, typeId);
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

int PopulationManager::GetNutrientStockpile() const
{
    return m_nutrientStockpile;
}

int PopulationManager::GetNutrientsRequired(const std::vector<ActiveEffect_t>& activeEffects) const
{
    return GrowthCalculator::ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), activeEffects);
}

void PopulationManager::ApplyGrowth(int nutrients, const std::vector<ActiveEffect_t>& activeEffects)
{
    m_nutrientStockpile += nutrients;

    const int required = GrowthCalculator::ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), activeEffects);
    if (m_nutrientStockpile >= required)
    {
        m_nutrientStockpile -= required;
        on_growth.emit();
    }
    else if (m_nutrientStockpile < 0)
    {
        m_nutrientStockpile = 0;
        on_starvation.emit();
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

    m_container.ApplyCompositionTargets(targets, GetDefaultPopType());
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

void PopulationManager::NotifyPopGained_()
{
    on_pop_gained.emit(GetSize());
}

void PopulationManager::NotifyPopLost_()
{
    on_pop_lost.emit(GetSize());
}

} // namespace ac
