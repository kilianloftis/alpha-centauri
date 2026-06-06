#include "game/faction/Pop.h"
#include "game/faction/Specialist.h"

namespace ac
{

Pop::Pop()
    : m_tileId(-1)
{
}

Pop::~Pop()
{
}

PopProduction_t Pop::GetProduction(const PopProduction_t& tileResources) const
{
    // Base implementation returns zeros - derived classes override
    return PopProduction_t{0, 0, 0, 0, 0};
}

WorkerPop::WorkerPop()
    : m_bIsTalent(false)
{
}

WorkerPop::~WorkerPop()
{
}

const char* WorkerPop::GetPopType() const
{
    return "Worker";
}

bool WorkerPop::IsWorker() const
{
    return true;
}

bool WorkerPop::IsDrone() const
{
    return false;
}

bool WorkerPop::IsSpecialist() const
{
    return false;
}

void WorkerPop::SetTileId(int tileId)
{
    m_tileId = tileId;
}

int WorkerPop::GetTileId() const
{
    return m_tileId;
}

PopProduction_t WorkerPop::GetProduction(const PopProduction_t& tileResources) const
{
    // Workers produce exactly what their tile provides
    return PopProduction_t{
        tileResources.nutrients,
        tileResources.energy,
        tileResources.minerals,
        0,  // no labs
        0   // no psych
    };
}

TalentPop::TalentPop()
{
    m_bIsTalent = true;
}

TalentPop::~TalentPop()
{
}

const char* TalentPop::GetPopType() const
{
    return "Talent";
}

PopProduction_t TalentPop::GetProduction(const PopProduction_t& tileResources) const
{
    // Talents produce what their tile provides + psych bonus
    return PopProduction_t{
        tileResources.nutrients,
        tileResources.energy,
        tileResources.minerals,
        0,  // no labs
        1   // +1 psych from being a talent
    };
}

DronePop::DronePop()
{
}

DronePop::~DronePop()
{
}

const char* DronePop::GetPopType() const
{
    return "Drone";
}

bool DronePop::IsWorker() const
{
    return false;
}

bool DronePop::IsDrone() const
{
    return true;
}

bool DronePop::IsSpecialist() const
{
    return false;
}

PopProduction_t DronePop::GetProduction(const PopProduction_t& tileResources) const
{
    // Drones produce nothing
    return PopProduction_t{0, 0, 0, 0, 0};
}

SpecialistPop::SpecialistPop(std::unique_ptr<Specialist> pSpecialist)
    : m_pSpecialist(std::move(pSpecialist))
{
}

SpecialistPop::~SpecialistPop()
{
}

const char* SpecialistPop::GetPopType() const
{
    return "Specialist";
}

bool SpecialistPop::IsWorker() const
{
    return false;
}

bool SpecialistPop::IsDrone() const
{
    return false;
}

bool SpecialistPop::IsSpecialist() const
{
    return true;
}

const Specialist* SpecialistPop::GetSpecialist() const
{
    return m_pSpecialist.get();
}

PopProduction_t SpecialistPop::GetProduction(const PopProduction_t& tileResources) const
{
    // Specialists ignore tile resources, produce based on their type
    (void)tileResources;  // Unused parameter
    PopProduction_t production{0, 0, 0, 0, 0};
    if (m_pSpecialist)
    {
        SpecialistOutput_t output = m_pSpecialist->GetOutput();
        production.nutrients = output.nutrients;
        production.energy = output.energy;
        production.minerals = output.minerals;
        production.labs = output.labs;
        production.psych = output.psych;
    }
    return production;
}

} // namespace ac
