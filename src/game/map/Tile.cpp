#include "game/map/Tile.h"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace ac
{

std::string ToString(Rockiness rockiness)
{
    switch (rockiness)
    {
        case Rockiness::Flat:    return "Flat";
        case Rockiness::Rolling: return "Rolling";
        case Rockiness::Rocky:   return "Rocky";
    }
    return "";
}

std::string ToString(Moisture moisture)
{
    switch (moisture)
    {
        case Moisture::Arid:  return "Arid";
        case Moisture::Moist: return "Moist";
        case Moisture::Wet:   return "Wet";
    }
    return "";
}

Tile::Tile()
    : m_x(0)
    , m_y(0)
    , m_moisture(Moisture::Arid)
    , m_baseMoisture(Moisture::Arid)
    , m_rockiness(Rockiness::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bHasFungus(false)
    , m_bWorked(false)
{
}

Tile::Tile(int x, int y)
    : m_x(x)
    , m_y(y)
    , m_moisture(Moisture::Arid)
    , m_baseMoisture(Moisture::Arid)
    , m_rockiness(Rockiness::Flat)
    , m_elevation(0)
    , m_bHasRiver(false)
    , m_bHasFungus(false)
    , m_bWorked(false)
{
}

Tile::~Tile()
{
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

void Tile::SetBaseMoisture(Moisture moisture)
{
    m_baseMoisture = moisture;
}

Moisture Tile::GetBaseMoisture() const
{
    return m_baseMoisture;
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

int Tile::GetElevationEnergySeed() const
{
    if (m_elevation < 0)
    {
        return 0;
    }
    return static_cast<int>(std::floor(m_elevation / 1000.0));
}

void Tile::SetHasRiver(bool bHasRiver)
{
    m_bHasRiver = bHasRiver;
}

bool Tile::GetHasRiver() const
{
    return m_bHasRiver;
}

void Tile::SetHasFungus(bool bHasFungus)
{
    m_bHasFungus = bHasFungus;
}

bool Tile::GetHasFungus() const
{
    return m_bHasFungus;
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

bool Tile::IsWorkerAssigned() const
{
    return m_bWorked;
}

bool Tile::HasFeature(std::string_view featureId) const
{
    if (ToString(m_rockiness) == featureId) return true;
    if (ToString(m_moisture)  == featureId) return true;
    if (m_bHasRiver  && featureId == "River")  return true;
    if (m_bHasFungus && featureId == "Fungus") return true;
    if (!m_landmark.empty() && m_landmark == featureId) return true;
    if (!m_bonus.empty()    && m_bonus    == featureId) return true;
    for (const std::string& id : m_improvements)
    {
        if (id == featureId) return true;
    }
    return false;
}

std::vector<std::string> Tile::GetFeatureIds() const
{
    std::vector<std::string> ids;
    ids.push_back(ToString(m_rockiness));
    ids.push_back(ToString(m_moisture));
    if (m_bHasRiver)
    {
        ids.push_back("River");
    }
    if (m_bHasFungus)
    {
        ids.push_back("Fungus");
    }
    if (HasLandmark())
    {
        ids.push_back(m_landmark);
    }
    if (HasBonus())
    {
        ids.push_back(m_bonus);
    }
    ids.insert(ids.end(), m_improvements.begin(), m_improvements.end());
    return ids;
}

} // namespace ac
