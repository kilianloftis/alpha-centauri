#include "game/faction/base/population/PopulationManager.h"
#include "game/population/calculators/GrowthCalculator.h"
#include "game/population/calculators/DroneCalculator.h"
#include "game/population/calculators/PopCompositionCalculator.h"
#include "game/population/calculators/PopTypeAvailabilityCalculator.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/population/pop-types/PopTypeRegistry.h"
#include "game/population/pop-types/GrowthConfigParser.h"
#include "game/faction/ResearchManager.h"
#include <iostream>
#include <stdexcept>
#include <utility>

namespace ac
{

PopulationManager::PopulationManager(const PopTypeRegistry& rPopTypeRegistry,
                                     const PopTypeAvailabilityCalculator& rPopTypeAvailabilityCalculator,
                                     const GrowthConfig_t& rGrowthConfig,
                                     DroneCalculator& rDroneCalculator,
                                     PopCompositionCalculator& rCompositionCalculator,
                                     const ResearchManager& rResearchManager,
                                     int initialSize)
    : m_container(rPopTypeRegistry, initialSize)
    , m_rRegistry(rPopTypeRegistry)
    , m_rAvailabilityCalculator(rPopTypeAvailabilityCalculator)
    , m_pResearch(&rResearchManager)
    , m_rGrowthConfig(rGrowthConfig)
    , m_rDroneCalculator(rDroneCalculator)
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
    m_pResearch = &rResearch;
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
    // Resolved like any conversion: creating a pop of an obsoleted type would seat something
    // no conversion path can produce, which is the asymmetry this package removed.
    m_container.AddPop(ResolveType_(typeId));
    NotifyPopGained_();
    m_riot.NotifyPopGrown(BuildRiotInputs_());
}

void PopulationManager::SetPopValuator(std::function<int(const Pop&)> valuator)
{
    m_popValuator = std::move(valuator);
}

Pop& PopulationManager::SelectDoomedPop_()
{
    // Specialists are taken last, so the comparison key leads with "is a specialist" — false
    // sorts first. Within a group the pop producing the least total resource goes; ties keep
    // the earliest, which makes the choice deterministic rather than allocation-order dependent.
    Pop* pDoomed = nullptr;
    std::pair<bool, int> worst;
    for (Pop& rPop : m_container.Pops())
    {
        const std::pair<bool, int> key{rPop.IsSpecialist(),
                                       m_popValuator ? m_popValuator(rPop) : 0};
        if (!pDoomed || key < worst)
        {
            pDoomed = &rPop;
            worst = key;
        }
    }
    if (!pDoomed)
    {
        throw std::runtime_error("PopulationManager::RemovePop: base has no population");
    }
    return *pDoomed;
}

void PopulationManager::RemovePop()
{
    Pop& rDoomed = SelectDoomedPop_();
    OnPopRemoved.Emit(rDoomed);
    m_container.Remove(rDoomed);

    // Targets are a function of base size, so a removal invalidates them immediately. Conquest,
    // a probe pop-kill and starvation all shrink a base mid-turn and used to leave the
    // drone/talent split describing the size the base no longer has until the next Population
    // stage. Not done on AddPop: the caller there names the type it wants, and reconciling
    // inside the add would silently overwrite it.
    //
    // Before OnPopLost, not after: observers of that signal — including EventBridge's
    // EvBaseLostPop, which is mod-facing — read the composition, and announcing the new size
    // alongside the old split just moves the staleness into them.
    MaybeRecalculateComposition_();
    NotifyPopLost_();
}

const PopTypeConfig_t& PopulationManager::ResolveType_(const std::string& typeId) const
{
    // The single place a requested pop type becomes an actual one. Every conversion path goes
    // through the obsolescence chain, so a pop always lands on the most current non-obsoleted
    // successor of what was asked for. Previously only the fallback path did this, and plain
    // ConvertTo installed the raw id — so it could seat a type the fallback path would refuse.
    return m_rAvailabilityCalculator.ResolveCurrentType(typeId,
                                                        m_pResearch->GetDiscoveredTechs());
}

void PopulationManager::ConvertResolved_(Pop& rPop, const std::string& typeId)
{
    m_container.ConvertTo(rPop, ResolveType_(typeId));
}

void PopulationManager::ConvertTo(Pop& rPop, const std::string& typeId)
{
    const bool wasSpecialist = rPop.IsSpecialist();
    ConvertResolved_(rPop, typeId);
    if (wasSpecialist || rPop.IsSpecialist())
    {
        MaybeRecalculateComposition_();
    }
}

void PopulationManager::ConvertToDefaultPopType(Pop& rPop)
{
    ConvertTo(rPop, GetDefaultPopType_());
}

void PopulationManager::ConvertToFallback(Pop& rPop)
{
    const PopTypeConfig_t* pCurrent = m_rRegistry.Find(rPop.GetPopType());
    if (!pCurrent)
    {
        throw std::runtime_error("PopulationManager::ConvertToFallback: current pop type not in "
                                 "registry: " + std::string(rPop.GetPopType()));
    }
    if (pCurrent->fallbackPopTypeId.empty())
    {
        throw std::runtime_error("PopulationManager::ConvertToFallback: pop type '"
                                 + pCurrent->id + "' has no fallback configured");
    }
    // Same resolution as any other conversion — ConvertTo owns the chain walk.
    ConvertTo(rPop, pCurrent->fallbackPopTypeId);
}

PopulationManager::BatchCompositionUpdate::BatchCompositionUpdate(PopulationManager& rPops)
    : m_rPops(rPops)
{
    ++m_rPops.m_compositionBatchDepth;
}

PopulationManager::BatchCompositionUpdate::~BatchCompositionUpdate()
{
    if (--m_rPops.m_compositionBatchDepth != 0 || !m_rPops.m_bCompositionDirty)
    {
        return;
    }
    m_rPops.m_bCompositionDirty = false;
    // Destructors are implicitly noexcept, and the deferred work here can throw: it reaches
    // GetDefaultPopType_ (registry) and ResolveType_ (obsolescence chain), both of which throw
    // on a config the registry cannot satisfy. Letting that escape calls std::terminate — and
    // this guard sits on the hot worker-assignment paths, so it would take the process down
    // rather than surface a config error. Report and swallow instead; the composition is left
    // stale, which is recoverable, unlike termination.
    try
    {
        m_rPops.RecalculateComposition();
    }
    catch (const std::exception& rError)
    {
        std::cerr << "BatchCompositionUpdate: deferred composition recalculation failed: "
                  << rError.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "BatchCompositionUpdate: deferred composition recalculation failed\n";
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

void PopulationManager::SetDroneInputSupplier(std::function<DroneInputs_t()> supplier)
{
    m_droneInputSupplier = std::move(supplier);
}

DroneInputs_t PopulationManager::BuildDroneInputs_() const
{
    if (m_droneInputSupplier)
    {
        return m_droneInputSupplier();
    }
    DroneInputs_t inputs;
    inputs.baseSize = m_container.GetSize();
    return inputs;
}

RiotConditionInputs_t PopulationManager::BuildRiotInputs_() const
{
    RiotConditionInputs_t inputs;
    inputs.droneCount = m_container.GetRiotContribution();
    inputs.talentCount = m_container.GetTalentCount();
    PopCompositionInputs_t compInputs;
    compInputs.targetDrones = m_rDroneCalculator.Calculate(BuildDroneInputs_());
    compInputs.psychOutput = m_container.ComputePsychOutput();
    inputs.targetTalents = m_rCompositionCalculator.Calculate(compInputs).targetTalents;
    return inputs;
}

void PopulationManager::RecalculateComposition()
{
    PopCompositionInputs_t inputs;
    inputs.targetDrones = m_rDroneCalculator.Calculate(BuildDroneInputs_());
    inputs.psychOutput = m_container.ComputePsychOutput();
    // TODO: supply faction talent modifiers once faction modifiers are accessible here
    const PopCompositionResult_t targets = m_rCompositionCalculator.Calculate(inputs);
    const PopCompositionConfig_t& rConfig = m_rCompositionCalculator.GetConfig();

    ApplyCompositionTargets(targets, rConfig.droneTypeId, rConfig.talentTypeId);
}

void PopulationManager::ApplyCompositionTargets(const PopCompositionResult_t& rTargets,
                                                const std::string& droneTypeId,
                                                const std::string& talentTypeId)
{
    // Reconciliation is population *policy*, so it lives here rather than in the container:
    // it decides which pops change and in what order, and every conversion it performs is
    // resolved through the obsolescence chain like any other.
    //
    // ConvertResolved_ rather than ConvertTo: we are already inside a recalculation, and
    // ConvertTo's specialist hook would re-enter it. Drones and talents are tile-workers, so
    // that hook does not fire for them today — but relying on that is how a recursion bug gets
    // introduced by a later pop type that is a specialist.
    const std::string& rDefaultTypeId = GetDefaultPopType_();

    // Demote surplus first, so the promotions below have plain workers to draw from.
    int currentDrones = m_container.GetDroneCount();
    for (Pop& rPop : m_container.Pops())
    {
        if (currentDrones <= rTargets.targetDrones)
        {
            break;
        }
        if (rPop.IsDrone())
        {
            ConvertResolved_(rPop, rDefaultTypeId);
            --currentDrones;
        }
    }

    int currentTalents = m_container.GetTalentCount();
    for (Pop& rPop : m_container.Pops())
    {
        if (currentTalents <= rTargets.targetTalents)
        {
            break;
        }
        if (rPop.IsTalent())
        {
            ConvertResolved_(rPop, rDefaultTypeId);
            --currentTalents;
        }
    }

    // TODO: this promotes to drones first. When plain workers are scarce the order decides who
    // gets promoted, and SMAC's rule for it is unknown. No config controls it - the parser
    // rejects a `precedence` key rather than accepting one it cannot honour.
    currentDrones = m_container.GetDroneCount();
    for (Pop& rPop : m_container.Pops())
    {
        if (currentDrones >= rTargets.targetDrones)
        {
            break;
        }
        if (rPop.IsPlainWorker())
        {
            ConvertResolved_(rPop, droneTypeId);
            ++currentDrones;
        }
    }

    currentTalents = m_container.GetTalentCount();
    for (Pop& rPop : m_container.Pops())
    {
        if (currentTalents >= rTargets.targetTalents)
        {
            break;
        }
        if (rPop.IsPlainWorker())
        {
            ConvertResolved_(rPop, talentTypeId);
            ++currentTalents;
        }
    }
}

void PopulationManager::MaybeRecalculateComposition_()
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

void PopulationManager::ForceRiot(int turns)
{
    m_riot.ForceRiot(turns);
}

void PopulationManager::CheckGoldenAgeEndOfTurn()
{
    GoldenAgeCalculator::Inputs_t inputs;
    inputs.droneCount = m_container.GetDroneCount();
    inputs.talentCount = m_container.GetTalentCount();
    // Plain workers, not GetWorkerCount(): that counts every tile-capable pop, so drones and
    // talents landed on both sides of the calculator's documented
    // "talents >= workers + specialists" rule. Counting talents against themselves made the
    // effective condition "every pop must be a talent" — far stricter than the stated rule.
    inputs.workerCount = m_container.GetPlainWorkerCount();
    inputs.specialistCount = m_container.GetSpecialistCount();
    m_goldenAge.Update(inputs);
}

bool PopulationManager::IsInGoldenAge() const
{
    return m_goldenAge.IsInGoldenAge();
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
