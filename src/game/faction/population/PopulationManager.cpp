#include "game/faction/population/PopulationManager.h"
#include "game/faction/population/PopCompositionCalculator.h"
#include "game/faction/population/PopTypeConfigParser.h"
#include "game/faction/population/PopTypeRegistry.h"

namespace ac
{

PopulationManager::PopulationManager()
    : m_pPopFactory(std::make_unique<PopFactory>())
    , m_maxSize(8)
    , m_growthRate(1)
{
    m_growth.on_growth.connect([this]() { AddPop(); });
    m_growth.on_starvation.connect([this]() { RemovePop(); });
}

PopulationManager::PopulationManager(int initialSize)
    : m_pPopFactory(std::make_unique<PopFactory>())
    , m_maxSize(8)
    , m_growthRate(1)
{
    m_growth.on_growth.connect([this]() { AddPop(); });
    m_growth.on_starvation.connect([this]() { RemovePop(); });

    if (initialSize > 0)
    {
        m_pops.reserve(static_cast<size_t>(initialSize));
    }
}

PopulationManager::~PopulationManager()
{
}

int PopulationManager::GetSize() const
{
    return static_cast<int>(m_pops.size());
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

int PopulationManager::ComputePsychOutput_() const
{
    static const PopProduction_t kNoTile = {};
    int total = 0;
    for (const auto& pPop : m_pops)
    {
        total += pPop->GetProduction(kNoTile).psych;
    }
    return total;
}

void PopulationManager::SetSize(int size)
{
    if (size > GetSize())
    {
        // Add pops
        while (GetSize() < size)
        {
            AddPop();
        }
    }
    else if (size < GetSize())
    {
        // Remove pops
        while (GetSize() > size)
        {
            RemovePop();
        }
    }
}

int PopulationManager::GetGrowthRate() const
{
    return m_growthRate;
}

bool PopulationManager::CanGrow() const
{
    return static_cast<int>(m_pops.size()) < m_maxSize;
}

const std::vector<std::unique_ptr<Pop>>& PopulationManager::GetPops() const
{
    return m_pops;
}

Pop* PopulationManager::GetPop(size_t index)
{
    if (index < m_pops.size())
    {
        return m_pops[index].get();
    }
    return nullptr;
}

int PopulationManager::GetWorkerCount() const
{
    return CountPops_([](const Pop* p) { return p->IsWorker() && !p->IsSpecialist(); });
}

int PopulationManager::GetTalentCount() const
{
    return CountPops_([](const Pop* p) { return p->IsWorker() && p->GetGoldenAgeContribution() > 0; });
}

int PopulationManager::GetDroneCount() const
{
    return CountPops_([](const Pop* p) { return p->IsDrone(); });
}

int PopulationManager::GetSpecialistCount() const
{
    return CountPops_([](const Pop* p) { return p->IsSpecialist(); });
}

void PopulationManager::AddPop()
{
    if (CanGrow())
    {
        m_pops.push_back(m_pPopFactory->CreatePop(GetDefaultPopType_()));
        NotifyPopGained_();
        m_riot.NotifyPopGrown(HasDroneRiot());
    }
}

void PopulationManager::RemovePop()
{
    if (!m_pops.empty())
    {
        m_pops.pop_back();
        NotifyPopLost_();
    }
}

void PopulationManager::ConvertTo(size_t index, const std::string& typeId)
{
    if (index >= m_pops.size())
    {
        return;
    }
    int tileId = m_pops[index]->GetTileId();
    auto pNewPop = m_pPopFactory->CreatePop(typeId);
    if (pNewPop)
    {
        pNewPop->SetTileId(tileId);
        m_pops[index] = std::move(pNewPop);
    }
}

void PopulationManager::SetRegistry(const PopTypeRegistry* pRegistry)
{
    m_pPopFactory->SetRegistry(pRegistry);

    // Populate reserved pops now that the registry is available
    if (m_pops.empty() && m_pops.capacity() > 0)
    {
        const size_t target = m_pops.capacity();
        for (size_t i = 0; i < target; i++)
        {
            auto pPop = m_pPopFactory->CreatePop(GetDefaultPopType_());
            if (pPop)
            {
                m_pops.push_back(std::move(pPop));
            }
        }
    }
}

int PopulationManager::GetMaxSize() const
{
    return m_maxSize;
}

void PopulationManager::SetMaxSize(int maxSize)
{
    m_maxSize = maxSize;
    // Trim excess pops if max size decreased
    bool bLostPop = false;
    while (static_cast<int>(m_pops.size()) > m_maxSize)
    {
        m_pops.pop_back();
        bLostPop = true;
    }
    if (bLostPop)
    {
        NotifyPopLost_();
    }
}

bool PopulationManager::HasDroneRiot() const
{
    if (m_pCompositionCalculator)
    {
        PopCompositionInputs inputs;
        inputs.baseSize   = GetSize();
        inputs.psychOutput = ComputePsychOutput_();
        // TODO: supply faction drone/talent modifiers once faction modifiers are accessible here
        const PopCompositionResult result = m_pCompositionCalculator->Calculate(inputs);
        return GetDroneCount() > result.targetTalents;
    }
    return GetDroneCount() > GetTalentCount();
}

bool PopulationManager::IsDestroyed() const
{
    return m_pops.empty();
}

void PopulationManager::AddDrone()
{
    // Convert a random worker to a drone
    for (size_t i = 0; i < m_pops.size(); i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist())
        {
            ConvertTo(i, "Drone");
            return;
        }
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
    inputs.baseSize    = GetSize();
    inputs.psychOutput = ComputePsychOutput_();
    // TODO: supply faction drone/talent modifiers once faction modifiers are accessible here
    const PopCompositionResult targets = m_pCompositionCalculator->Calculate(inputs);

    // Convert excess drones back to workers first
    int currentDrones = GetDroneCount();
    for (size_t i = 0; i < m_pops.size() && currentDrones > targets.targetDrones; i++)
    {
        if (m_pops[i]->IsDrone())
        {
            ConvertTo(i, GetDefaultPopType_());
            currentDrones--;
        }
    }

    // Convert excess talents back to workers
    int currentTalents = GetTalentCount();
    for (size_t i = 0; i < m_pops.size() && currentTalents > targets.targetTalents; i++)
    {
        if (m_pops[i]->IsWorker() && m_pops[i]->GetGoldenAgeContribution() > 0)
        {
            ConvertTo(i, GetDefaultPopType_());
            currentTalents--;
        }
    }

    // Convert workers to drones to reach target
    currentDrones = GetDroneCount();
    for (size_t i = 0; i < m_pops.size() && currentDrones < targets.targetDrones; i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist() && m_pops[i]->GetGoldenAgeContribution() == 0)
        {
            ConvertTo(i, "Drone");
            currentDrones++;
        }
    }

    // Convert workers to talents to reach target
    currentTalents = GetTalentCount();
    for (size_t i = 0; i < m_pops.size() && currentTalents < targets.targetTalents; i++)
    {
        if (m_pops[i]->IsWorker() && !m_pops[i]->IsSpecialist() && m_pops[i]->GetGoldenAgeContribution() == 0)
        {
            ConvertTo(i, "Talent");
            currentTalents++;
        }
    }
}

int PopulationManager::CountPops_(bool (*predicate)(const Pop*)) const
{
    int count = 0;
    for (const auto& pPop : m_pops)
    {
        if (predicate(pPop.get()))
        {
            count++;
        }
    }
    return count;
}

void PopulationManager::AccumulateGrowth(int nutrientsPerTurn)
{
    GrowthInputs_t inputs;
    inputs.baseSize           = GetSize();
    inputs.nutrientsPerTurn   = nutrientsPerTurn;
    inputs.growthRateModifier = m_growthRate;
    m_growth.Accumulate(inputs);
}

void PopulationManager::CheckRiotEndOfTurn()
{
    m_riot.Update(HasDroneRiot());
}

void PopulationManager::CheckGoldenAgeEndOfTurn()
{
    GoldenAgeCalculator::Inputs_t inputs;
    inputs.droneCount      = GetDroneCount();
    inputs.talentCount     = GetTalentCount();
    inputs.workerCount     = GetWorkerCount();
    inputs.specialistCount = GetSpecialistCount();
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

const RiotCalculator& PopulationManager::GetRiot() const
{
    return m_riot;
}

const GrowthCalculator& PopulationManager::GetGrowth() const
{
    return m_growth;
}

const GoldenAgeCalculator& PopulationManager::GetGoldenAge() const
{
    return m_golden_age;
}

} // namespace ac
