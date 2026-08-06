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

PopulationManager::PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                                     const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                                     const GrowthConfig_t& rGrowthConfig,
                                     PopCompositionCalculator& rCompositionCalculator,
                                     const ResearchManager& rResearchManager,
                                     int initialSize)
    : m_container(rPopTypeRegistry,
                  rPopTypeAvailabilityCalculator,
                  rResearchManager,
                  initialSize)
    , m_rRegistry(rPopTypeRegistry)
    , m_rGrowthConfig(rGrowthConfig)
    , m_rCompositionCalculator(rCompositionCalculator)
    // The cap comes from pop_growth.json; there is no second, compiled-in default to drift.
    , m_maxSize(rGrowthConfig.maxBaseSize)
    , m_nutrientStockpile(0)
    , m_riot(OnWillRiot, OnIsRioting, OnRiotEnded)
    , m_goldenAge(OnGoldenAgeStarted, OnGoldenAgeEnded)
{
}

PopulationManager::~PopulationManager()
{
}

void PopulationManager::RebindResearch(const ResearchManager& rResearch)
{
    m_container.RebindResearch(rResearch);
}

int PopulationManager::GetSize() const
{
    return m_container.GetSize();
}

const std::string& PopulationManager::GetDefaultPopType_() const
{
    return m_rRegistry.GetDefault().id;
}

bool PopulationManager::CanGrow() const
{
    return m_container.GetSize() < m_maxSize;
}

void PopulationManager::AddPop()
{
    AddPop(GetDefaultPopType_());
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
    const bool wasSpecialist = rPop.IsSpecialist();
    m_container.ConvertTo(rPop, typeId);
    if (wasSpecialist || rPop.IsSpecialist())
    {
        MaybeRecalculateCompositionAfterSpecialistChange_();
    }
}

void PopulationManager::ConvertToDefaultPopType(Pop& rPop)
{
    ConvertTo(rPop, GetDefaultPopType_());
}

void PopulationManager::ConvertToFallback(Pop& rPop)
{
    const bool wasSpecialist = rPop.IsSpecialist();
    m_container.ConvertToFallback(rPop);
    if (wasSpecialist || rPop.IsSpecialist())
    {
        MaybeRecalculateCompositionAfterSpecialistChange_();
    }
}

PopulationManager::BatchCompositionUpdate::BatchCompositionUpdate(PopulationManager& rPops)
    : m_rPops(rPops)
{
    ++m_rPops.m_compositionBatchDepth;
}

PopulationManager::BatchCompositionUpdate::~BatchCompositionUpdate()
{
    if (--m_rPops.m_compositionBatchDepth == 0 && m_rPops.m_bCompositionDirty)
    {
        m_rPops.m_bCompositionDirty = false;
        m_rPops.RecalculateComposition();
    }
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

void PopulationManager::SetNutrientStockpile(int amount)
{
    m_nutrientStockpile = amount;
}

int PopulationManager::GetNutrientsRequired(const BaseEffects_t& rBaseEffects) const
{
    return GrowthCalculator::ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), rBaseEffects);
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

    const int required = GrowthCalculator::ComputeNutrientsRequired(m_rGrowthConfig, GetSize(), rBaseEffects);
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
    PopCompositionInputs compInputs;
    compInputs.baseSize = m_container.GetSize();
    compInputs.psychOutput = m_container.ComputePsychOutput();
    inputs.targetTalents = m_rCompositionCalculator.Calculate(compInputs).targetTalents;
    return inputs;
}

void PopulationManager::RecalculateComposition()
{
    PopCompositionInputs inputs;
    inputs.baseSize = m_container.GetSize();
    inputs.psychOutput = m_container.ComputePsychOutput();
    // TODO: supply faction drone/talent modifiers once faction modifiers are accessible here
    const PopCompositionResult targets = m_rCompositionCalculator.Calculate(inputs);
    const PopCompositionConfig_t& rConfig = m_rCompositionCalculator.GetConfig();

    m_container.ApplyCompositionTargets(targets, GetDefaultPopType_(),
                                        rConfig.droneTypeId, rConfig.talentTypeId);
}

void PopulationManager::MaybeRecalculateCompositionAfterSpecialistChange_()
{
    if (m_compositionBatchDepth > 0)
    {
        m_bCompositionDirty = true;
        return;
    }
    RecalculateComposition();
}

void PopulationManager::CheckRiotEndOfTurn()
{
    m_riot.Update(BuildRiotInputs_());
}

void PopulationManager::ForceRiot()
{
    m_riot.ForceRiot();
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
