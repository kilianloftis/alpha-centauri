#include "game/map/Tile.h"
#include <algorithm>
#include <cmath>

namespace ac
{

Tile::Tile()
    : m_x(0)
    , m_y(0)
    , m_moisture(Moisture::Arid)
    , m_rockiness(Rockiness::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bWorked(false)
    , m_workedByBaseId(-1)
{
}

Tile::Tile(int x, int y)
    : m_x(x)
    , m_y(y)
    , m_moisture(Moisture::Arid)
    , m_rockiness(Rockiness::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bWorked(false)
    , m_workedByBaseId(-1)
{
}

Tile::~Tile()
{
}

void Tile::SetPosition(int x, int y)
{
    m_x = x;
    m_y = y;
}

int Tile::GetX() const
{
    return m_x;
}

int Tile::GetY() const
{
    return m_y;
}

void Tile::SetMoisture(Moisture moisture)
{
    m_moisture = moisture;
}

Moisture Tile::GetMoisture() const
{
    return m_moisture;
}

void Tile::SetRockiness(Rockiness rockiness)
{
    m_rockiness = rockiness;
}

Rockiness Tile::GetRockiness() const
{
    return m_rockiness;
}

void Tile::SetElevation(int elevation)
{
    m_elevation = elevation;
}

int Tile::GetElevation() const
{
    return m_elevation;
}

int Tile::GetNutrientProduction() const
{
    return CalculateNutrients_() + CalculateBonusNutrients_();
}

int Tile::GetMineralProduction() const
{
    return CalculateMinerals_() + CalculateBonusMinerals_();
}

int Tile::GetEnergyProduction() const
{
    return CalculateEnergy_() + CalculateBonusEnergy_();
}

void Tile::SetHasRiver(bool bHasRiver)
{
    m_bHasRiver = bHasRiver;
}

bool Tile::GetHasRiver() const
{
    return m_bHasRiver;
}

void Tile::SetLandmark(const std::string& landmarkId)
{
    m_landmark = landmarkId;
}

void Tile::RemoveLandmark()
{
    m_landmark.clear();
}

bool Tile::HasLandmark() const
{
    return !m_landmark.empty();
}

const std::string& Tile::GetLandmark() const
{
    return m_landmark;
}

void Tile::AddImprovement(const std::string& improvementId)
{
    if (!HasImprovement(improvementId))
    {
        m_improvements.push_back(improvementId);
    }
}

void Tile::RemoveImprovement(const std::string& improvementId)
{
    auto it = std::remove(m_improvements.begin(), m_improvements.end(), improvementId);
    m_improvements.erase(it, m_improvements.end());
}

bool Tile::HasImprovement(const std::string& improvementId) const
{
    return std::find(m_improvements.begin(), m_improvements.end(), improvementId) != m_improvements.end();
}

const std::vector<std::string>& Tile::GetImprovements() const
{
    return m_improvements;
}

void Tile::SetBonus(const std::string& bonusId)
{
    m_bonus = bonusId;
}

void Tile::RemoveBonus()
{
    m_bonus.clear();
}

bool Tile::HasBonus() const
{
    return !m_bonus.empty();
}

const std::string& Tile::GetBonus() const
{
    return m_bonus;
}

void Tile::SetWorked(bool bWorked) const
{
    m_bWorked = bWorked;
}

bool Tile::IsWorked() const
{
    return m_bWorked;
}

void Tile::AssignWorker(int baseId)
{
    m_bWorked = true;
    m_workedByBaseId = baseId;
}

void Tile::UnassignWorker()
{
    m_bWorked = false;
    m_workedByBaseId = -1;
}

bool Tile::IsWorkerAssigned() const
{
    return m_bWorked;
}

int Tile::GetWorkedByBaseId() const
{
    return m_workedByBaseId;
}

int Tile::CalculateBonusNutrients_() const
{
    // TODO: Look up bonus in TileBonusRegistry when registry is available at this level
    // For now, bonus values are calculated at the Base level where registry is accessible
    return 0;
}

int Tile::CalculateBonusMinerals_() const
{
    // TODO: Look up bonus in TileBonusRegistry when registry is available at this level
    return 0;
}

int Tile::CalculateBonusEnergy_() const
{
    // TODO: Look up bonus in TileBonusRegistry when registry is available at this level
    return 0;
}

int Tile::CalculateNutrients_() const
{
    // TODO: Define game rules for nutrient calculation from moisture
    switch (m_moisture)
    {
        case Moisture::Wet:
            return 2;
        case Moisture::Moist:
            return 1;
        case Moisture::Arid:
        default:
            return 0;
    }
}

int Tile::CalculateMinerals_() const
{
    // TODO: Define game rules for mineral calculation from rockiness
    switch (m_rockiness)
    {
        case Rockiness::Rocky:
            return 2;
        case Rockiness::Rolling:
            return 1;
        case Rockiness::Flat:
        default:
            return 0;
    }
}

int Tile::CalculateEnergy_() const
{
    // TODO: Define game rules for energy calculation from elevation
    // Elevation in meters (-4000 to 4000), normalize to 0-4 range
    // Rivers may provide energy bonus
    if (m_elevation < 0) {
        return 0;
    }
    
    int baseEnergy = static_cast<int>(floor(m_elevation / 1000.0));
    if (m_bHasRiver)
    {
        baseEnergy += 1;
    }
    return baseEnergy;
}

} // namespace ac
