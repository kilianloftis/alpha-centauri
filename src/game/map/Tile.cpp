#include "game/map/Tile.h"
#include "game/map/ImprovementConfigParser.h"
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

const std::vector<std::string>& AllTerrainFeatureIds()
{
    // Built from the same ToString mappings HasFeature matches against, so the list can't
    // drift from the actual matching logic.
    static const std::vector<std::string> ids = {
        ToString(Rockiness::Flat), ToString(Rockiness::Rolling), ToString(Rockiness::Rocky),
        ToString(Moisture::Arid), ToString(Moisture::Moist), ToString(Moisture::Wet),
        "River", "Fungus",
    };
    return ids;
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

void Tile::AddImprovement(const ImprovementConfig_t& rConfig)
{
    if (!HasImprovement(rConfig.id))
    {
        m_improvements.push_back(&rConfig);
    }
}

void Tile::RemoveImprovement(std::string_view improvementId)
{
    auto it = std::remove_if(m_improvements.begin(), m_improvements.end(),
                             [&](const ImprovementConfig_t* pConfig)
                             {
                                 return pConfig->id == improvementId;
                             });
    m_improvements.erase(it, m_improvements.end());
}

bool Tile::HasImprovement(std::string_view improvementId) const
{
    return std::any_of(m_improvements.begin(), m_improvements.end(),
                       [&](const ImprovementConfig_t* pConfig)
                       {
                           return pConfig->id == improvementId;
                       });
}

const std::vector<const ImprovementConfig_t*>& Tile::GetImprovements() const
{
    return m_improvements;
}

bool Tile::HasFeature(std::string_view featureId) const
{
    if (ToString(m_rockiness) == featureId) return true;
    if (ToString(m_moisture)  == featureId) return true;
    if (m_bHasRiver  && featureId == "River")  return true;
    if (m_bHasFungus && featureId == "Fungus") return true;
    return HasImprovement(featureId);
}

std::vector<std::string> Tile::GetTerrainFeatureIds() const
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
    return ids;
}

} // namespace ac
