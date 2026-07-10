#include "game/faction/base/population/PopulationManager.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/faction/ResearchManager.h"
#include <stdexcept>

namespace ac
{

PopulationManager::PopulationManager(const PopTypeRegistry* pPopTypeRegistry,
                                     const PopTypeAvailabilityCalculator* pPopTypeAvailabilityCalculator,
                                     const GrowthConfig_t* pGrowthConfig,
                                     PopCompositionCalculator* pCompositionCalculator,
                                     const ResearchManager* pResearchManager,
                                     int initialSize)
    : m_container(pPopTypeRegistry,
                  pPopTypeAvailabilityCalculator,
                  pResearchManager,
                  initialSize)
    , m_pRegistry(pPopTypeRegistry)
    , m_pGrowthConfig(pGrowthConfig)
    , m_pCompositionCalculator(pCompositionCalculator)
    , m_maxSize(pGrowthConfig ? pGrowthConfig->maxBaseSize : 7)
    , m_nutrientStockpile(0)
    , m_riot(OnWillRiot, OnIsRioting, OnRiotEnded)
    , m_goldenAge(OnGoldenAgeStarted, OnGoldenAgeEnded)
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
    AddPop(GetDefaultPopType());
}

void PopulationManager::AddPop(const std::string& typeId)
{
    if (!CanGrow())
    {
        throw std::runtime_error("Cannot add pop: base is at max size");
    }
    m_container.AddPop(typeId);
    NotifyPopGained_();
    m_riot.NotifyPopGrown(BuildRiotInputs_());
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

void PopulationManager::ConvertToFallback(Pop& rPop)
{
    m_container.ConvertToFallback(rPop);
}

int PopulationManager::GetMaxSize() const
{
    return m_maxSize;
}

void PopulationManager::SetMaxSize(int maxSize)
{
    // TODO: Hab Complex / Habitation Dome buildings should call this to raise the
    // population cap (SMAC: 7 without Hab Complex, 14 without Hab Dome).
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

int PopulationManager::GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const
{
    return GrowthCalculator::ComputeNutrientsRequired(*m_pGrowthConfig, GetSize(), rBaseEffects);
}

void PopulationManager::ApplyGrowth(int nutrients, const BaseEffects_t& rBaseEffects)
{
    m_nutrientStockpile += nutrients;

    if (m_nutrientStockpile < 0)
    {
        m_nutrientStockpile = 0;
        OnStarvation.Emit();
        return;
    }

    // At the population cap, bank nutrients but do not spend the growth threshold
    // on a pop that cannot appear. Hab buildings raise the cap via SetMaxSize.
    if (!CanGrow())
    {
        return;
    }

    const int required = GrowthCalculator::ComputeNutrientsRequired(*m_pGrowthConfig, GetSize(), rBaseEffects);
    if (m_nutrientStockpile >= required)
    {
        m_nutrientStockpile -= required;
        OnGrowth.Emit();
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
    const PopCompositionConfig_t& rConfig = m_pCompositionCalculator->GetConfig();

    m_container.ApplyCompositionTargets(targets, GetDefaultPopType(),
                                        rConfig.droneTypeId, rConfig.talentTypeId);
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
    m_goldenAge.Update(inputs);
}

void PopulationManager::NotifyPopGained_()
{
    OnPopGained.Emit(GetSize());
}

void PopulationManager::NotifyPopLost_()
{
    OnPopLost.Emit(GetSize());
}

} // namespace ac
